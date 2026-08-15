#include "qsim/qaoa.hpp"

#include "qsim/gates.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace qsim {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

MaxCutGraph::MaxCutGraph(int num_nodes) : num_nodes_(std::max(2, num_nodes)) {
    auto_layout();
}

void MaxCutGraph::set_num_nodes(int n) {
    num_nodes_ = std::max(2, n);
    // Remove edges referring to out-of-range nodes
    edges_.erase(std::remove_if(edges_.begin(), edges_.end(),
                                [this](const Edge& e) { return e.u >= num_nodes_ || e.v >= num_nodes_; }),
                 edges_.end());
    auto_layout();
}

void MaxCutGraph::auto_layout() {
    node_positions_.resize(static_cast<std::size_t>(num_nodes_));
    const float radius = 80.0f;
    const float center_x = 100.0f;
    const float center_y = 100.0f;
    for (int i = 0; i < num_nodes_; ++i) {
        double angle = 2.0 * kPi * static_cast<double>(i) / num_nodes_ - kPi / 2.0;
        node_positions_[static_cast<std::size_t>(i)] = {
            center_x + static_cast<float>(radius * std::cos(angle)),
            center_y + static_cast<float>(radius * std::sin(angle))
        };
    }
}

void MaxCutGraph::set_node_position(int node, float x, float y) {
    if (node >= 0 && node < static_cast<int>(node_positions_.size())) {
        node_positions_[static_cast<std::size_t>(node)] = {x, y};
    }
}

void MaxCutGraph::add_edge(int u, int v, double weight) {
    if (u < 0 || u >= num_nodes_ || v < 0 || v >= num_nodes_ || u == v) return;
    if (has_edge(u, v)) return;
    edges_.push_back({std::min(u, v), std::max(u, v), weight});
}

bool MaxCutGraph::remove_edge(int u, int v) {
    Edge target{std::min(u, v), std::max(u, v), 1.0};
    auto it = std::find(edges_.begin(), edges_.end(), target);
    if (it != edges_.end()) {
        edges_.erase(it);
        return true;
    }
    return false;
}

bool MaxCutGraph::has_edge(int u, int v) const {
    Edge target{std::min(u, v), std::max(u, v), 1.0};
    return std::find(edges_.begin(), edges_.end(), target) != edges_.end();
}

double MaxCutGraph::evaluate_cut(int bitstring) const {
    double cut = 0.0;
    for (const auto& e : edges_) {
        int u_val = qubit_value(bitstring, num_nodes_, e.u);
        int v_val = qubit_value(bitstring, num_nodes_, e.v);
        if (u_val != v_val) {
            cut += e.weight;
        }
    }
    return cut;
}

double MaxCutGraph::max_cut_exact(std::vector<int>* best_bitstrings) const {
    const int dim = 1 << num_nodes_;
    double best_cut = -1.0;
    std::vector<int> best;

    for (int b = 0; b < dim; ++b) {
        double c = evaluate_cut(b);
        if (c > best_cut + 1e-9) {
            best_cut = c;
            best.clear();
            best.push_back(b);
        } else if (std::abs(c - best_cut) <= 1e-9) {
            best.push_back(b);
        }
    }

    if (best_bitstrings) {
        *best_bitstrings = best;
    }
    return best_cut;
}

Hamiltonian MaxCutGraph::cost_hamiltonian() const {
    Hamiltonian h(num_nodes_);
    // H_C = sum_{(u,v)} w/2 * (I - Z_u Z_v)
    for (const auto& e : edges_) {
        const double w_half = e.weight * 0.5;
        h.add_term(PauliTerm(w_half, {})); // w/2 * I
        h.add_term(PauliTerm(-w_half, {{e.u, PauliOp::Z}, {e.v, PauliOp::Z}})); // -w/2 * Z_u Z_v
    }
    return h;
}

Hamiltonian MaxCutGraph::qaoa_hamiltonian() const {
    Hamiltonian h(num_nodes_);
    // H_min = -H_C = sum_{(u,v)} w/2 * (Z_u Z_v - I)
    for (const auto& e : edges_) {
        const double w_half = e.weight * 0.5;
        h.add_term(PauliTerm(w_half, {{e.u, PauliOp::Z}, {e.v, PauliOp::Z}}));
        h.add_term(PauliTerm(-w_half, {}));
    }
    return h;
}

MaxCutGraph MaxCutGraph::triangle() {
    MaxCutGraph g(3);
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    g.add_edge(2, 0);
    return g;
}

MaxCutGraph MaxCutGraph::cycle4() {
    MaxCutGraph g(4);
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    g.add_edge(2, 3);
    g.add_edge(3, 0);
    return g;
}

MaxCutGraph MaxCutGraph::bowtie5() {
    MaxCutGraph g(5);
    // Node 2 is the center vertex
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    g.add_edge(2, 0);
    g.add_edge(2, 3);
    g.add_edge(3, 4);
    g.add_edge(4, 2);
    return g;
}

MaxCutGraph MaxCutGraph::regular6() {
    MaxCutGraph g(6);
    // 3-regular graph on 6 vertices (Prism graph)
    g.add_edge(0, 1);
    g.add_edge(1, 2);
    g.add_edge(2, 0);
    g.add_edge(3, 4);
    g.add_edge(4, 5);
    g.add_edge(5, 3);
    g.add_edge(0, 3);
    g.add_edge(1, 4);
    g.add_edge(2, 5);
    return g;
}

QAOAMaxCutAnsatz::QAOAMaxCutAnsatz(MaxCutGraph graph, int layers)
    : graph_(std::move(graph)), layers_(std::max(1, layers)) {}

std::vector<std::string> QAOAMaxCutAnsatz::param_names() const {
    std::vector<std::string> names;
    names.reserve(static_cast<std::size_t>(2 * layers_));
    for (int l = 0; l < layers_; ++l) {
        std::ostringstream g_name, b_name;
        g_name << "gamma" << (l + 1);
        b_name << "beta" << (l + 1);
        names.push_back(g_name.str());
        names.push_back(b_name.str());
    }
    return names;
}

std::vector<double> QAOAMaxCutAnsatz::default_initial_params() const {
    std::vector<double> init;
    init.reserve(static_cast<std::size_t>(2 * layers_));
    for (int l = 0; l < layers_; ++l) {
        init.push_back(0.5 + 0.1 * l); // gamma
        init.push_back(0.3 - 0.05 * l); // beta
    }
    return init;
}

Circuit QAOAMaxCutAnsatz::build_circuit(const std::vector<double>& params) const {
    Circuit c;
    c.num_qubits = graph_.num_nodes();
    int op_id = 1;
    int col = 0;

    // Step 1: Initial state |+>^n
    for (int q = 0; q < c.num_qubits; ++q) {
        c.add_op({GateKind::H, col, q, -1, -1, 0.0, 0.0, 0.0, op_id++});
    }
    col++;

    // Step 2: p layers of (Cost Unitary + Mixer Unitary)
    for (int l = 0; l < layers_; ++l) {
        const std::size_t g_idx = static_cast<std::size_t>(2 * l);
        const std::size_t b_idx = static_cast<std::size_t>(2 * l + 1);
        const double gamma = (g_idx < params.size()) ? params[g_idx] : 0.5;
        const double beta = (b_idx < params.size()) ? params[b_idx] : 0.3;

        // Cost unitary: for each edge (u,v), apply CNOT -> Rz(-gamma * weight) -> CNOT
        for (const auto& e : graph_.edges()) {
            const double rz_angle = -gamma * e.weight;
            c.add_op({GateKind::CNOT, col, e.u, e.v, -1, 0.0, 0.0, 0.0, op_id++});
            col++;
            c.add_op({GateKind::Rz, col, e.v, -1, -1, rz_angle, 0.0, 0.0, op_id++});
            col++;
            c.add_op({GateKind::CNOT, col, e.u, e.v, -1, 0.0, 0.0, 0.0, op_id++});
            col++;
        }

        // Mixer unitary: Rx(2 * beta) on each qubit
        for (int q = 0; q < c.num_qubits; ++q) {
            c.add_op({GateKind::Rx, col, q, -1, -1, 2.0 * beta, 0.0, 0.0, op_id++});
        }
        col++;
    }

    return c;
}

}  // namespace qsim
