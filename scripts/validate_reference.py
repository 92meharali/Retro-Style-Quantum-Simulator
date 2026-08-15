#!/usr/bin/env python3
"""
Quantum Simulator Cross-Validation Script
Validates VQE and QAOA Hamiltonians, expectation values, parameter-shift gradients,
and optimization targets against exact linear algebra and standard reference formulations.
Has zero external dependencies (pure Python with optional NumPy/Qiskit if present).
"""

import cmath
import math
import sys

def mat_mul(A, B):
    n = len(A)
    m = len(B[0])
    k = len(B)
    C = [[0.0 + 0.0j for _ in range(m)] for _ in range(n)]
    for i in range(n):
        for j in range(m):
            s = 0.0 + 0.0j
            for p in range(k):
                s += A[i][p] * B[p][j]
            C[i][j] = s
    return C

def mat_vec_mul(A, v):
    n = len(A)
    k = len(v)
    res = [0.0 + 0.0j for _ in range(n)]
    for i in range(n):
        s = 0.0 + 0.0j
        for p in range(k):
            s += A[i][p] * v[p]
        res[i] = s
    return res

def vec_dot(u, v):
    return sum(u[i].conjugate() * v[i] for i in range(len(u)))

def kron(A, B):
    rA, cA = len(A), len(A[0])
    rB, cB = len(B), len(B[0])
    res = [[0.0 + 0.0j for _ in range(cA * cB)] for _ in range(rA * rB)]
    for i in range(rA):
        for j in range(cA):
            for k in range(rB):
                for l in range(cB):
                    res[i * rB + k][j * cB + l] = A[i][j] * B[k][l]
    return res

def mat_add(A, B, scaleB=1.0):
    n = len(A)
    m = len(A[0])
    return [[A[i][j] + scaleB * B[i][j] for j in range(m)] for i in range(n)]

# Standard 2x2 matrices
I2 = [[1.0 + 0j, 0j], [0j, 1.0 + 0j]]
X = [[0j, 1.0 + 0j], [1.0 + 0j, 0j]]
Y = [[0j, -1.0j], [1.0j, 0j]]
Z = [[1.0 + 0j, 0j], [0j, -1.0 + 0j]]

def ry(theta):
    c = math.cos(theta / 2.0)
    s = math.sin(theta / 2.0)
    return [[c + 0j, -s + 0j], [s + 0j, c + 0j]]

def rx(theta):
    c = math.cos(theta / 2.0)
    s = math.sin(theta / 2.0)
    return [[c + 0j, -1.0j * s], [-1.0j * s, c + 0j]]

def rz(theta):
    return [[cmath.exp(-1.0j * theta / 2.0), 0j],
            [0j, cmath.exp(1.0j * theta / 2.0)]]

def cnot():
    m = [[0.0 + 0j for _ in range(4)] for _ in range(4)]
    m[0][0] = 1.0 + 0j
    m[1][1] = 1.0 + 0j
    m[2][3] = 1.0 + 0j
    m[3][2] = 1.0 + 0j
    return m

def ground_state_power_iter(H):
    dim = len(H)
    # Estimate max row sum
    max_row = max(sum(abs(H[i][j]) for j in range(dim)) for i in range(dim))
    shift = max_row + 10.0
    
    # B = shift * I - H
    B = [[-H[i][j] for j in range(dim)] for i in range(dim)]
    for i in range(dim):
        B[i][i] += shift

    # Multi-start power iteration
    best_rayleigh = 1e9
    trials = []
    # Basis states
    for b in range(min(dim, 4)):
        v = [1.0 + 0j if i == b else 0j for i in range(dim)]
        trials.append(v)
    trials.append([1.0 / math.sqrt(dim) + 0j for _ in range(dim)])
    trials.append([(1.0 if i % 2 == 0 else -1.0) / math.sqrt(dim) + 0j for i in range(dim)])

    for v in trials:
        for _ in range(100):
            next_v = mat_vec_mul(B, v)
            norm = math.sqrt(sum(abs(x) ** 2 for x in next_v))
            if norm < 1e-12:
                break
            v = [x / norm for x in next_v]
        
        Hv = mat_vec_mul(H, v)
        rayleigh = vec_dot(v, Hv).real
        if rayleigh < best_rayleigh:
            best_rayleigh = rayleigh
            
    return best_rayleigh

def validate_pauli_algebra():
    print("=" * 70)
    print(" [1] PAULI ALGEBRA & HAMILTONIAN VALIDATION")
    print("=" * 70)
    
    # H = X0*X1 + Z0*Z1
    H_xx_zz = mat_add(kron(X, X), kron(Z, Z))
    e0_exact = ground_state_power_iter(H_xx_zz)
    
    print(f"Target: H = X (x) X + Z (x) Z")
    print(f"Exact Ground State Energy E0: {e0_exact:.10f}")
    assert abs(e0_exact - (-2.0)) < 1e-8, f"E0 must be -2.0, got {e0_exact}"
    print("  -> PASSED: Ground state energy is exactly -2.0000000000\n")

    # STO-3G H2 Molecule at R = 0.7414 A
    g0 = -1.0523732
    g1 = 0.3979374
    g2 = -0.3979374
    g3 = -0.0112801
    g4 = 0.1809312
    g5 = 0.1809312
    
    dim = 4
    H_h2 = [[0.0 + 0j for _ in range(dim)] for _ in range(dim)]
    H_h2 = mat_add(H_h2, kron(I2, I2), g0)
    H_h2 = mat_add(H_h2, kron(Z, I2), g1)
    H_h2 = mat_add(H_h2, kron(I2, Z), g2)
    H_h2 = mat_add(H_h2, kron(Z, Z), g3)
    H_h2 = mat_add(H_h2, kron(X, X), g4)
    H_h2 = mat_add(H_h2, kron(Y, Y), g5)
    
    h2_e0 = ground_state_power_iter(H_h2)
    print(f"Target: STO-3G H2 Molecule (R = 0.7414 A)")
    print(f"Exact Ground State Energy E0: {h2_e0:.10f} Ha")
    assert abs(h2_e0 - (-1.91537)) < 1e-3, f"H2 E0 discrepancy: {h2_e0}"
    print("  -> PASSED: H2 Ground state verified\n")

def validate_parameter_shift_rule():
    print("=" * 70)
    print(" [2] PARAMETER-SHIFT RULE ANALYTICAL GRADIENT VALIDATION")
    print("=" * 70)
    
    H = mat_add(kron(X, X), kron(Z, Z))
    
    def circuit_state(t0, t1):
        psi0 = [1.0 + 0j, 0j, 0j, 0j]
        u_ry = kron(ry(t0), ry(t1))
        u_cx = cnot()
        u_total = mat_mul(u_cx, u_ry)
        return mat_vec_mul(u_total, psi0)
    
    def energy(t0, t1):
        psi = circuit_state(t0, t1)
        H_psi = mat_vec_mul(H, psi)
        return vec_dot(psi, H_psi).real
    
    t0, t1 = 0.785398, 1.250000
    s = math.pi / 2.0
    
    # Parameter shift
    grad_ps_0 = (energy(t0 + s, t1) - energy(t0 - s, t1)) / (2.0 * math.sin(s))
    grad_ps_1 = (energy(t0, t1 + s) - energy(t0, t1 - s)) / (2.0 * math.sin(s))
    
    # Finite differences
    eps = 1e-7
    grad_fd_0 = (energy(t0 + eps, t1) - energy(t0 - eps, t1)) / (2.0 * eps)
    grad_fd_1 = (energy(t0, t1 + eps) - energy(t0, t1 - eps)) / (2.0 * eps)
    
    diff0 = abs(grad_ps_0 - grad_fd_0)
    diff1 = abs(grad_ps_1 - grad_fd_1)
    
    print(f"Point: (theta0={t0:.4f}, theta1={t1:.4f})")
    print(f"Parameter-Shift Gradient: [{grad_ps_0:.8f}, {grad_ps_1:.8f}]")
    print(f"Finite-Difference Grad:   [{grad_fd_0:.8f}, {grad_fd_1:.8f}]")
    print(f"Max Discrepancy:          {max(diff0, diff1):.2e}")
    assert max(diff0, diff1) < 1e-5, "Parameter shift gradient error too high"
    print("  -> PASSED: Parameter-shift analytical gradient matches numerical exact derivative\n")

def validate_qaoa_maxcut():
    print("=" * 70)
    print(" [3] QAOA MAXCUT GRAPH & COMBINATORIAL VALIDATION")
    print("=" * 70)
    
    # Triangle Graph: (0,1), (1,2), (2,0)
    def triangle_cut(b):
        q0 = (b >> 2) & 1
        q1 = (b >> 1) & 1
        q2 = (b >> 0) & 1
        return (q0 != q1) + (q1 != q2) + (q2 != q0)
    
    cuts = [triangle_cut(b) for b in range(8)]
    print(f"Triangle Graph (3 vertices, 3 edges)")
    print(f"Bitstring Cut Values: {cuts}")
    print(f"Max Possible Cut: {max(cuts)}")
    assert max(cuts) == 2, "Triangle max cut must be 2"
    
    # 4-Cycle Graph
    def cycle4_cut(b):
        q0 = (b >> 3) & 1
        q1 = (b >> 2) & 1
        q2 = (b >> 1) & 1
        q3 = (b >> 0) & 1
        return (q0 != q1) + (q1 != q2) + (q2 != q3) + (q3 != q0)
    
    c4_cuts = [cycle4_cut(b) for b in range(16)]
    max_c4 = max(c4_cuts)
    optimal_c4 = [b for b in range(16) if c4_cuts[b] == max_c4]
    print(f"4-Cycle Graph Max Cut: {max_c4} (Optimal Bitstrings: {optimal_c4})")
    assert max_c4 == 4, "4-Cycle max cut must be 4"
    assert optimal_c4 == [5, 10], "Optimal partitions must be 0101 (5) and 1010 (10)"
    print("  -> PASSED: MaxCut combinatorial values and optimal partitions verified\n")

def main():
    print("\n" + "#" * 70)
    print(" QUANTUM SIMULATOR CROSS-VALIDATION SUITE")
    print(" Pure-Python Reference Verification (Zero External Dependencies)")
    print("#" * 70 + "\n")
    
    validate_pauli_algebra()
    validate_parameter_shift_rule()
    validate_qaoa_maxcut()
    
    print("=" * 70)
    print(" SUMMARY: ALL CROSS-VALIDATION CHECKS PASSED (100% NUMERICAL MATCH)")
    print("=" * 70 + "\n")

if __name__ == "__main__":
    main()
