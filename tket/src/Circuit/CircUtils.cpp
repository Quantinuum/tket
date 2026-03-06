// Copyright Quantinuum
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tket/Circuit/CircUtils.hpp"

#include <cmath>
#include <complex>
#include <sstream>
#include <vector>

#include "tket/Circuit/CircPool.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/ConjugationBox.hpp"
#include "tket/Diagonalisation/Diagonalisation.hpp"
#include "tket/Gate/GatePtr.hpp"
#include "tket/Gate/GateUnitaryMatrixImplementations.hpp"
#include "tket/Gate/Rotation.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Utils/Expression.hpp"
#include "tket/Utils/MatrixAnalysis.hpp"
#include "tket/Utils/UnitID.hpp"
#include "tklog/TketLog.hpp"

namespace tket {

Eigen::Matrix2cd get_matrix(const Circuit &circ, const Vertex &vert) {
  const Op_ptr op = circ.get_Op_ptr_from_Vertex(vert);
  if (op->get_type() != OpType::TK1) {
    throw BadOpType("Cannot compute matrix from gate", op->get_type());
  }
  std::vector<Expr> ps = op->get_params();
  ps.push_back(0);
  return get_matrix_from_tk1_angles(ps);
}

Eigen::Matrix2cd get_matrix_from_circ(const Circuit &circ) {
  if (circ.n_qubits() != 1)
    throw CircuitInvalidity(
        "Getting Matrix: expected 1 qubit circuit, found " +
        std::to_string(circ.n_qubits()));
  Complex factor = std::exp(i_ * PI * eval_expr(circ.get_phase()).value());
  VertexVec qpath = circ.qubit_path_vertices(circ.all_qubits()[0]);
  unsigned N = qpath.size();
  if (N == 2) return factor * Eigen::Matrix2cd::Identity();
  Eigen::Matrix2cd m = get_matrix(circ, qpath[N - 2]);
  for (unsigned x = N - 3; x >= 1; --x) {
    m = unitary_product2(m, get_matrix(circ, qpath[x]));
  }
  return factor * m;
}

Eigen::Matrix4cd get_matrix_from_2qb_circ(const Circuit &circ) {
  std::vector<QPathDetailed> all_paths = circ.all_qubit_paths();
  std::map<Vertex, Eigen::Matrix4cd> v_to_op;
  Eigen::Matrix4cd cnot, tonc, swap;
  // clang-format off
    cnot << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0, 1,
            0, 0, 1, 0;
    tonc << 1, 0, 0, 0,
            0, 0, 0, 1,
            0, 0, 1, 0,
            0, 1, 0, 0;
    swap << 1, 0, 0, 0,
            0, 0, 1, 0,
            0, 1, 0, 0,
            0, 0, 0, 1;
  // clang-format on

  for (unsigned uqb = 0; uqb < 2; uqb++) {
    for (QPathDetailed::iterator it = all_paths[uqb].begin();
         it != all_paths[uqb].end(); ++it) {
      const Op_ptr o = circ.get_Op_ptr_from_Vertex(it->first);
      switch (o->get_type()) {
        case OpType::Input:
        case OpType::Create:
        case OpType::Output:
        case OpType::Discard: {
          v_to_op[it->first] = Eigen::Matrix4cd::Identity();
          break;
        }
        case OpType::SWAP: {
          if (uqb == 0) v_to_op[it->first] = swap;
          break;
        }
        case OpType::CX: {
          if (uqb == 0) {
            if (it->second == 0) {
              v_to_op[it->first] = cnot;
            } else {
              v_to_op[it->first] = tonc;
            }
          }
          break;
        }
        case OpType::TK2: {
          auto params = o->get_params();
          TKET_ASSERT(params.size() == 3);
          v_to_op[it->first] = get_matrix_from_2qb_circ(
              CircPool::normalised_TK2_using_CX(
                  params[0], params[1], params[2]));
          break;
        }
        default: {
          if (o->get_desc().is_gate() && circ.n_in_edges(it->first) == 1 &&
              circ.n_out_edges(it->first) == 1) {
            const Op_ptr g = o;
            std::vector<Expr> ps = as_gate_ptr(g)->get_tk1_angles();
            Eigen::Matrix2cd mat = get_matrix_from_tk1_angles(ps);
            if (uqb == 0) {
              v_to_op[it->first] =
                  Eigen::kroneckerProduct(mat, Eigen::Matrix2cd::Identity());
            } else {
              v_to_op[it->first] =
                  Eigen::kroneckerProduct(Eigen::Matrix2cd::Identity(), mat);
            }
          } else
            throw BadOpType("Cannot obtain matrix from op", o->get_type());
        }
      }
    }
  }
  Eigen::Matrix4cd m = Eigen::Matrix4cd::Identity();
  SliceVec slices = circ.get_slices();
  for (const Slice &s : slices) {
    for (const Vertex &v : s) {
      m = unitary_product2(v_to_op[v], m);
    }
  }
  return std::exp(i_ * PI * eval_expr(circ.get_phase()).value()) * m;
}

Circuit two_qubit_canonical(const Eigen::Matrix4cd &U, OpType target_2qb_gate) {
  if (!is_unitary(U)) {
    throw std::invalid_argument(
        "Non-unitary matrix passed to two_qubit_canonical");
  }

  auto [K1, A, K2] = get_information_content(U);

  K1 /= pow(K1.determinant(), 0.25);
  K2 /= pow(K2.determinant(), 0.25);
  auto [a, b, c] = A;

  // Decompose single qubits
  auto [K1a, K1b] = kronecker_decomposition(K1);
  auto [K2a, K2b] = kronecker_decomposition(K2);

  Circuit result(2);

  std::vector<double> angles_q0 = tk1_angles_from_unitary(K2a);
  std::vector<double> angles_q1 = tk1_angles_from_unitary(K2b);
  result.add_op<unsigned>(
      OpType::TK1, {angles_q0.begin(), angles_q0.end() - 1}, {0});
  result.add_op<unsigned>(
      OpType::TK1, {angles_q1.begin(), angles_q1.end() - 1}, {1});

  switch (target_2qb_gate) {
    case OpType::TK2:
      result.append(CircPool::TK2_using_normalised_TK2(a, b, c));
      break;
    case OpType::CX:
      result.append(CircPool::TK2_using_CX(a, b, c));
      break;
    default:
      throw std::invalid_argument("target_2qb_gate must be CX or TK2.");
  }

  angles_q0 = tk1_angles_from_unitary(K1a);
  angles_q1 = tk1_angles_from_unitary(K1b);
  result.add_op<unsigned>(
      OpType::TK1, {angles_q0.begin(), angles_q0.end() - 1}, {0});
  result.add_op<unsigned>(
      OpType::TK1, {angles_q1.begin(), angles_q1.end() - 1}, {1});

  // this fixes phase if decomposition is exact
  Eigen::Matrix4cd V = get_matrix_from_2qb_circ(result).adjoint();
  Eigen::Matrix4cd reminder = unitary_product2(V, U);
  const Complex phase = reminder(0, 0);  // reminder = phase * I
  result.add_phase(arg(phase) / PI);
  return result;
}

// Factorize U as VD where V corresponds to a 2-CX circuit and
// D = diag(z, z*, z*, z). Return V and z.
static std::pair<Eigen::Matrix4cd, Complex> decompose_VD(
    const Eigen::Matrix4cd &U) {
  if (!is_unitary(U)) {
    throw std::invalid_argument("Non-unitary matrix passed to decompose_VD");
  }

  // The calculations below are derived from the proof of Proposition V.2 in
  // https://arxiv.org/abs/quant-ph/0308033.

  Eigen::Matrix4cd u = U / pow(U.determinant(), 0.25);
  Complex a = u(3, 0) * u(0, 3) - u(2, 0) * u(1, 3) - u(1, 0) * u(2, 3) +
              u(0, 0) * u(3, 3);
  Complex b = u(3, 1) * u(0, 2) - u(2, 1) * u(1, 2) - u(1, 1) * u(2, 2) +
              u(0, 1) * u(3, 2);
  // Now we want to find z such that |z|=1 and (az* - bz) is real.
  // The numerical stability of this function is a concern when a is close to
  // -b*. This problem can be demonstrated in artificially constructed
  // examples (passing unitaries very close to, but not quite, the identity to
  // the functions below). In these cases the product VD (or DV) may not
  // approximate U to within Eigen's default tolerance. Is there a way to
  // dodge this issue?
  Complex w = a + std::conj(b);
  double d = std::abs(w);
  // If w = 0 then we can set z = 1.
  Complex z = (d < EPS) ? 1. : w / d;
  Complex z0 = sqrt(z);
  Complex z1 = std::conj(z0);
  Eigen::Matrix4cd V = U;
  V.col(0) *= z1;
  V.col(1) *= z0;
  V.col(2) *= z0;
  V.col(3) *= z1;
  return {V, z0};
}

static void replace_TK2_2CX(Circuit &circ) {
  VertexList bin;
  BGL_FORALL_VERTICES(v, circ.dag, DAG) {
    if (circ.get_OpType_from_Vertex(v) != OpType::TK2) continue;
    auto params = circ.get_Op_ptr_from_Vertex(v)->get_params();
    TKET_ASSERT(params.size() == 3);
    // Rounding errors can accumulate here; warn if so:
    if (!equiv_0(params[2], 4, 1e-6)) {
      std::stringstream ss;
      ss << "Rounding errors in CX decomposition: ZZPhase parameter = "
         << params[2] << " when it should be 0 (mod 4). Ignoring.";
      tket_log()->warn(ss.str());
    }
    Circuit sub = CircPool::approx_TK2_using_2xCX(params[0], params[1]);
    bin.push_back(v);
    circ.substitute(sub, v, Circuit::VertexDeletion::No);
  }
  TKET_ASSERT(bin.size() == 1);
  circ.remove_vertices(
      bin, Circuit::GraphRewiring::No, Circuit::VertexDeletion::Yes);
}

std::pair<Circuit, Complex> decompose_2cx_VD(const Eigen::Matrix4cd &U) {
  auto [V, z0] = decompose_VD(U);
  Circuit circ = two_qubit_canonical(V);
  replace_TK2_2CX(circ);
  return {circ, z0};
}

std::pair<Circuit, Complex> decompose_2cx_DV(const Eigen::Matrix4cd &U) {
  auto [V, z0] = decompose_VD(U.adjoint());
  V.adjointInPlace();
  Circuit circ = two_qubit_canonical(V);
  replace_TK2_2CX(circ);
  return {circ, std::conj(z0)};
}

Circuit phase_gadget(unsigned n_qubits, const Expr &t, CXConfigType cx_config) {
  return pauli_gadget(
      SpSymPauliTensor(DensePauliMap(n_qubits, Pauli::Z), t), cx_config);
}

Circuit pauli_gadget(SpSymPauliTensor paulis, CXConfigType cx_config) {
  Circuit circ;
  for (const std::pair<const Qubit, Pauli> &qp : paulis.string)
    circ.add_qubit(qp.first);
  if (SpPauliString(paulis.string) == SpPauliString()) {
    circ.add_phase(-paulis.coeff / 2);
    return circ;
  }
  Circuit compute(circ);
  Circuit action(circ);
  qubit_vector_t qubits;
  for (const std::pair<const Qubit, Pauli> &qp : paulis.string) {
    switch (qp.second) {
      case Pauli::I:
        break;
      case Pauli::X:
        compute.add_op<Qubit>(OpType::H, {qp.first});
        qubits.push_back(qp.first);
        break;
      case Pauli::Y:
        compute.add_op<Qubit>(OpType::V, {qp.first});
        qubits.push_back(qp.first);
        break;
      case Pauli::Z:
        qubits.push_back(qp.first);
        break;
    }
  }
  unsigned n_qubits = qubits.size();
  switch (cx_config) {
    case CXConfigType::Snake: {
      for (unsigned i = n_qubits - 1; i != 0; --i) {
        unsigned j = i - 1;
        compute.add_op<Qubit>(OpType::CX, {qubits[i], qubits[j]});
      }
      action.add_op<Qubit>(OpType::Rz, paulis.coeff, {qubits[0]});
      break;
    }
    case CXConfigType::Star: {
      for (unsigned i = n_qubits - 1; i != 0; --i) {
        compute.add_op<Qubit>(OpType::CX, {qubits[i], qubits[0]});
      }
      action.add_op<Qubit>(OpType::Rz, paulis.coeff, {qubits[0]});
      break;
    }
    case CXConfigType::Tree: {
      unsigned complete_layers = floor(log2(n_qubits));
      unsigned dense_end = pow(2, complete_layers);
      for (unsigned i = 0; i < n_qubits - dense_end; i++)
        compute.add_op<Qubit>(
            OpType::CX, {qubits[dense_end + i], qubits[dense_end - 1 - i]});
      for (unsigned step_size = 1; step_size < dense_end; step_size *= 2) {
        for (unsigned i = 0; i < dense_end; i += 2 * step_size)
          compute.add_op<Qubit>(OpType::CX, {qubits[i + step_size], qubits[i]});
      }
      action.add_op<Qubit>(OpType::Rz, paulis.coeff, {qubits[0]});
      break;
    }
    case CXConfigType::MultiQGate: {
      int sign_correction = 1;
      for (int q = n_qubits - 1; q > 0; q -= 2) {
        if (q - 1 > 0) {
          unsigned i = q, j = q - 1;
          // this is only equal to the CX decompositions above
          // up to phase, but phase differences are cancelled out by
          // its dagger XXPhase(-1/2) below.
          compute.add_op<Qubit>(OpType::H, {qubits[i]});
          compute.add_op<Qubit>(OpType::H, {qubits[j]});
          compute.add_op<Qubit>(
              OpType::XXPhase3, 0.5, {qubits[i], qubits[j], qubits[0]});
          sign_correction *= -1;
        } else {
          unsigned i = q;
          compute.add_op<Qubit>(OpType::CX, {qubits[i], qubits[0]});
        }
      }
      action.add_op<Qubit>(
          OpType::Rz, sign_correction * paulis.coeff, {qubits[0]});
      break;
    }
  }
  // ConjugationBox components must be in the default register
  compute.flatten_registers();
  action.flatten_registers();
  ConjugationBox box(
      std::make_shared<CircBox>(compute), std::make_shared<CircBox>(action));
  circ.add_box(box, circ.all_qubits());
  return circ;
}

Circuit pauli_gadget_pair(
    SpSymPauliTensor paulis0, SpSymPauliTensor paulis1,
    CXConfigType cx_config) {
  Circuit circ;
  for (const std::pair<const Qubit, Pauli> &qp : paulis0.string)
    circ.add_qubit(qp.first);
  for (const std::pair<const Qubit, Pauli> &qp : paulis1.string)
    circ.add_qubit(qp.first, false);
  if (SpPauliString(paulis0.string) == SpPauliString{}) {
    circ.append(pauli_gadget(paulis1, cx_config));
    circ.add_phase(-paulis0.coeff / 2);
    return circ;
  } else if (SpPauliString(paulis1.string) == SpPauliString{}) {
    circ.append(pauli_gadget(paulis0, cx_config));
    circ.add_phase(-paulis1.coeff / 2);
    return circ;
  }
  paulis0.compress();
  paulis1.compress();
  Circuit u(circ);
  Circuit v(circ);

  // Reduce the overlap down to at most 1 qubit, which may be matching or
  // mismatching; allow both gadgets to build on that qubit
  SpPauliStabiliser p0stab(paulis0.string);
  SpPauliStabiliser p1stab(paulis1.string);
  u.append(reduce_overlap_of_paulis(p0stab, p1stab, cx_config, true).first);
  paulis0 = SpSymPauliTensor(p0stab) * SpSymPauliTensor({}, paulis0.coeff);
  paulis1 = SpSymPauliTensor(p1stab) * SpSymPauliTensor({}, paulis1.coeff);

  /*
   * Combine circuits to give final result
   */
  v.append(pauli_gadget(paulis0));
  v.append(pauli_gadget(paulis1));
  // ConjugationBox components must be in the default register
  qubit_vector_t all_qubits = u.all_qubits();
  u.flatten_registers();
  v.flatten_registers();
  ConjugationBox cjbox(
      std::make_shared<CircBox>(u), std::make_shared<CircBox>(v));
  circ.add_box(cjbox, all_qubits);
  return circ;
}

void replace_CX_with_TK2(Circuit &c) {
  static const Op_ptr cx = std::make_shared<Gate>(OpType::CX);
  c.substitute_all(CircPool::CX_using_TK2(), cx);
}

Circuit with_TK2(Gate_ptr op) {
  std::vector<Expr> params = op->get_params();
  unsigned n = op->n_qubits();
  if (n == 0) {
    Circuit c(0);
    if (op->get_type() == OpType::Phase) {
      c.add_phase(op->get_params()[0]);
    }
    return c;
  } else if (n == 1) {
    Circuit c(1);
    c.add_op(op, std::vector<unsigned>{0});
    return c;
  } else if (n == 2 && op->free_symbols().empty()) {
    Eigen::Matrix4cd U = op->get_unitary();
    auto [K1, A, K2] = get_information_content(U);
    // Decompose single qubits
    auto [K1a, K1b] = kronecker_decomposition(K1);
    auto [K2a, K2b] = kronecker_decomposition(K2);
    Circuit c(2);
    std::vector<double> angles_K1a = tk1_angles_from_unitary(K1a);
    std::vector<double> angles_K1b = tk1_angles_from_unitary(K1b);
    std::vector<double> angles_K2a = tk1_angles_from_unitary(K2a);
    std::vector<double> angles_K2b = tk1_angles_from_unitary(K2b);
    c.add_op<unsigned>(
        OpType::TK1, {angles_K2a.begin(), angles_K2a.end() - 1}, {0});
    c.add_op<unsigned>(
        OpType::TK1, {angles_K2b.begin(), angles_K2b.end() - 1}, {1});
    double alpha = std::get<0>(A);
    double beta = std::get<1>(A);
    double gamma = std::get<2>(A);

    c.append(CircPool::TK2_using_normalised_TK2(alpha, beta, gamma));

    c.add_op<unsigned>(
        OpType::TK1, {angles_K1a.begin(), angles_K1a.end() - 1}, {0});
    c.add_op<unsigned>(
        OpType::TK1, {angles_K1b.begin(), angles_K1b.end() - 1}, {1});

    // Correct phase by computing the unitary and comparing with U:
    Eigen::Matrix4cd V_K1 = Eigen::KroneckerProduct(
        get_matrix_from_tk1_angles(
            {angles_K1a[0], angles_K1a[1], angles_K1a[2], 0}),
        get_matrix_from_tk1_angles(
            {angles_K1b[0], angles_K1b[1], angles_K1b[2], 0}));
    Eigen::Matrix4cd V_A =
        internal::GateUnitaryMatrixImplementations::TK2(alpha, beta, gamma);
    Eigen::Matrix4cd V_K2 = Eigen::KroneckerProduct(
        get_matrix_from_tk1_angles(
            {angles_K2a[0], angles_K2a[1], angles_K2a[2], 0}),
        get_matrix_from_tk1_angles(
            {angles_K2b[0], angles_K2b[1], angles_K2b[2], 0}));
    Eigen::Matrix4cd V_adj = unitary_product3(V_K1, V_A, V_K2).adjoint();
    Eigen::Matrix4cd R = unitary_product2(V_adj, U);
    const Complex phase = R(0, 0);  // R = phase * I
    c.add_phase(arg(phase) / PI);

    return c;
  }
  // Now the non-trivial cases.
  switch (op->get_type()) {
    case OpType::ISWAP:
      return CircPool::ISWAP_using_TK2(params[0]);
    case OpType::ISWAPMax:
      return CircPool::ISWAPMax_using_TK2();
    case OpType::PhasedISWAP:
      return CircPool::PhasedISWAP_using_TK2(params[0], params[1]);
    case OpType::XXPhase:
      return CircPool::XXPhase_using_TK2(params[0]);
    case OpType::YYPhase:
      return CircPool::YYPhase_using_TK2(params[0]);
    case OpType::ZZPhase:
      return CircPool::ZZPhase_using_TK2(params[0]);
    case OpType::NPhasedX:
      return CircPool::NPhasedX_using_PhasedX(n, params[0], params[1]);
    case OpType::ESWAP:
      return CircPool::ESWAP_using_TK2(params[0]);
    case OpType::FSim:
      return CircPool::FSim_using_TK2(params[0], params[1]);
    case OpType::CRx:
      return CircPool::CRx_using_TK2(params[0]);
    case OpType::CRy:
      return CircPool::CRy_using_TK2(params[0]);
    case OpType::CRz:
      return CircPool::CRz_using_TK2(params[0]);
    case OpType::CU1:
      return CircPool::CU1_using_TK2(params[0]);
    case OpType::XXPhase3:
      return CircPool::XXPhase3_using_TK2(params[0]);
    case OpType::AAMS:
      return CircPool::AAMS_using_TK2(params[0], params[1], params[2]);
    case OpType::CCX:
    case OpType::CSWAP:
    case OpType::BRIDGE:
    case OpType::CU3:
    case OpType::PhaseGadget: {
      // As a first, inefficient, solution, decompose these into CX and then
      // replace each CX with a TK2 (and some single-qubit gates).
      // TODO Find more efficient decompositions for these gates.
      Circuit c = with_CX(op);
      replace_CX_with_TK2(c);
      return c;
    }
    default:
      throw CircuitInvalidity("Cannot decompose " + op->get_name());
  }
}

Circuit with_CX(Gate_ptr op) {
  OpType optype = op->get_type();
  std::vector<Expr> params = op->get_params();
  unsigned n = op->n_qubits();
  if (n == 0) {
    Circuit c(0);
    if (op->get_type() == OpType::Phase) {
      c.add_phase(op->get_params()[0]);
    }
    return c;
  } else if (n == 1) {
    Circuit c(1);
    c.add_op(op, std::vector<unsigned>{0});
    return c;
  }
  switch (optype) {
    case OpType::CX: {
      Circuit c(2);
      c.add_op(op, std::vector<unsigned>{0, 1});
      return c;
    }
    case OpType::CCX:
      return CircPool::CCX_normal_decomp();
    case OpType::CY:
      return CircPool::CY_using_CX();
    case OpType::CZ:
      return CircPool::CZ_using_CX();
    case OpType::CH:
      return CircPool::CH_using_CX();
    case OpType::CV:
      return CircPool::CV_using_CX();
    case OpType::CVdg:
      return CircPool::CVdg_using_CX();
    case OpType::CSX:
      return CircPool::CSX_using_CX();
    case OpType::CSXdg:
      return CircPool::CSXdg_using_CX();
    case OpType::CS:
      return CircPool::CS_using_CX();
    case OpType::CSdg:
      return CircPool::CSdg_using_CX();
    case OpType::CRz:
      return CircPool::CRz_using_CX(params[0]);
    case OpType::CRx:
      return CircPool::CRx_using_CX(params[0]);
    case OpType::CRy:
      return CircPool::CRy_using_CX(params[0]);
    case OpType::CU1:
      return CircPool::CU1_using_CX(params[0]);
    case OpType::CU3:
      return CircPool::CU3_using_CX(params[0], params[1], params[2]);
    case OpType::PhaseGadget: {
      Circuit c = phase_gadget(n, params[0], CXConfigType::Snake);
      c.decompose_boxes_recursively();
      return c;
    }
    case OpType::SWAP:
      return CircPool::SWAP_using_CX_0();
    case OpType::CSWAP:
      return CircPool::CSWAP_using_CX();
    case OpType::BRIDGE:
      return CircPool::BRIDGE_using_CX_0();
    case OpType::ECR:
      return CircPool::ECR_using_CX();
    case OpType::ISWAP:
      return CircPool::ISWAP_using_CX(params[0]);
    case OpType::ZZMax:
      return CircPool::ZZMax_using_CX();
    case OpType::XXPhase:
      return CircPool::XXPhase_using_CX(params[0]);
    case OpType::YYPhase:
      return CircPool::YYPhase_using_CX(params[0]);
    case OpType::ZZPhase:
      return CircPool::ZZPhase_using_CX(params[0]);
    case OpType::TK2:
      return CircPool::TK2_using_CX(params[0], params[1], params[2]);
    case OpType::XXPhase3:
      return CircPool::XXPhase3_using_CX(params[0]);
    case OpType::ESWAP:
      return CircPool::ESWAP_using_CX(params[0]);
    case OpType::FSim:
      return CircPool::FSim_using_CX(params[0], params[1]);
    case OpType::Sycamore:
      return CircPool::FSim_using_CX(1. / 2., 1. / 6.);
    case OpType::ISWAPMax:
      return CircPool::ISWAPMax_using_CX();
    case OpType::PhasedISWAP:
      return CircPool::PhasedISWAP_using_CX(params[0], params[1]);
    case OpType::NPhasedX:
      return CircPool::NPhasedX_using_PhasedX(n, params[0], params[1]);
    case OpType::AAMS:
      return CircPool::AAMS_using_CX(params[0], params[1], params[2]);
    case OpType::TwinPhasedX:
      return CircPool::NPhasedX_using_PhasedX(2, params[0], params[1]);
    case OpType::PhasedXX:
      return CircPool::PhasedXX_using_CX(params[0], params[1]);
    default:
      throw CircuitInvalidity("Cannot decompose " + op->get_name());
  }
}

#define CNXTYPE(n) \
  (((n) == 2) ? OpType::CX : ((n) == 3) ? OpType::CCX : OpType::CnX)
#define CNZTYPE(n) (((n) == 2) ? OpType::CZ : OpType::CnZ)
#define CNYTYPE(n) (((n) == 2) ? OpType::CY : OpType::CnY)
#define CNRYTYPE(n) (((n) == 2) ? OpType::CRy : OpType::CnRy)
#define CNRXTYPE(n) (((n) == 2) ? OpType::CRx : OpType::CnRx)
#define CNRZTYPE(n) (((n) == 2) ? OpType::CRz : OpType::CnRz)
/**
 * Construct a circuit representing CnU1.
 */
static Circuit CnU1(unsigned n_controls, Expr lambda) {
  Gate_ptr u1_gate = as_gate_ptr(get_op_ptr(OpType::U1, lambda));
  // Use the gray code method if lambda contains symbols
  // The gray code decomp also produces less CXs when n_controls is 3 or 4
  if (eval_expr(lambda) == std::nullopt || n_controls == 3 || n_controls == 4) {
    return CircPool::CnU_gray_code_decomp(n_controls, u1_gate);
  } else {
    return CircPool::CnU_linear_depth_decomp(
        n_controls, u1_gate->get_unitary());
  }
}

/**
 * @brief Returns the controlled version of a ConjugationBox
 * The returned circuit is box free
 * @param op assumed to be ConjugationBox
 * @param n_controls
 * @param args qubits where the box was originally placed, assumed to be qubits
 * from the default register.
 * @return Circuit
 */
static Circuit controlled_conjugation_box(
    const Op_ptr &op, unsigned n_controls, const unit_vector_t &args) {
  const ConjugationBox &conj_box = static_cast<const ConjugationBox &>(*op);
  unsigned n_targets = args.size();
  Op_ptr compute = conj_box.get_compute();
  Op_ptr action = conj_box.get_action();
  std::optional<Op_ptr> uncompute_opt = conj_box.get_uncompute();
  Op_ptr uncompute = uncompute_opt ? uncompute_opt.value() : compute->dagger();
  qubit_vector_t all_args(n_controls + n_targets);
  qubit_vector_t target_args(n_targets);
  for (unsigned i = 0; i < n_controls; i++) {
    all_args[i] = Qubit(i);
  }
  for (unsigned i = 0; i < n_targets; i++) {
    TKET_ASSERT(
        args[i].reg_name() == q_default_reg() && args[i].reg_dim() == 1);
    all_args[n_controls + i] = Qubit(n_controls + args[i].index()[0]);
    target_args[i] = Qubit(n_controls + args[i].index()[0]);
  }
  Circuit circ;
  for (const Qubit &q : all_args) {
    circ.add_qubit(q);
  }
  circ.add_op(compute, target_args);
  QControlBox controlled_action(action, n_controls);
  circ.add_box(controlled_action, all_args);
  circ.add_op(uncompute, target_args);
  circ.decompose_boxes_recursively();
  return circ;
}

static Circuit with_controls_symbolic(const Circuit &c, unsigned n_controls) {
  if (c.n_bits() != 0 || !c.is_simple()) {
    throw CircuitInvalidity("Only default qubit register allowed");
  }

  Circuit c1(c);
  // Replace wire swaps with SWAP gates
  c1.replace_all_implicit_wire_swaps();

  // Dispose of the trivial case
  if (n_controls == 0) {
    return c1;
  }

  static const OpTypeSet multiq_gate_set = {
      OpType::CX,   OpType::CCX,  OpType::CnX, OpType::CRy,
      OpType::CnRy, OpType::CZ,   OpType::CnZ, OpType::CY,
      OpType::CnY,  OpType::CnRx, OpType::CnRz};

  unsigned c_n_qubits = c1.n_qubits();

  // 1. Rebase to {CX, CCX, CnX, CnRy, CnRx, CnRz} and single-qubit gates
  VertexList bin;
  BGL_FORALL_VERTICES(v, c1.dag, DAG) {
    Op_ptr op = c1.get_Op_ptr_from_Vertex(v);
    OpType optype = op->get_type();
    if (is_gate_type(optype)) {
      if (is_projective_type(optype)) {
        throw CircuitInvalidity("Projective operations present");
      }
      if (is_single_qubit_type(optype)) {
        continue;
      }
      if (multiq_gate_set.find(optype) != multiq_gate_set.end()) {
        continue;
      }
      Circuit replacement;
      if (optype == OpType::PhaseGadget) {
        replacement = phase_gadget(
            op->n_qubits(), op->get_params()[0], CXConfigType::Snake);
        if (replacement.n_gates() > 0) {
          TKET_ASSERT(
              replacement.n_gates() == 1 &&
              replacement.count_gates(OpType::ConjugationBox) == 1);
        }
      } else {
        replacement = with_CX(as_gate_ptr(op));
      }
      c1.substitute(replacement, v, Circuit::VertexDeletion::No);
      bin.push_back(v);
    } else if (is_box_type(optype) && optype != OpType::ConjugationBox) {
      throw CircuitInvalidity("Undecomposed boxes present");
    }
  }
  c1.remove_vertices(
      bin, Circuit::GraphRewiring::No, Circuit::VertexDeletion::Yes);

  // Capture the phase. We may adjust this during replacements below.
  Expr a = c1.get_phase();

  // 2. Replace all gates with controlled versions
  Circuit c2(n_controls + c_n_qubits);
  for (Circuit::CommandIterator cit = c1.begin(); cit != c1.end(); ++cit) {
    Op_ptr op = cit->get_op_ptr();
    OpType optype = op->get_type();
    unit_vector_t args = cit->get_args();
    unsigned n_args = args.size();
    if (optype == OpType::Barrier) {
      qubit_vector_t barrier_args(n_args);
      for (unsigned i = 0; i < n_args; i++) {
        barrier_args[i] = Qubit(n_controls + args[i].index()[0]);
      }
      c2.add_op(op, barrier_args);
      continue;
    }
    if (optype == OpType::ConjugationBox) {
      c2.append(controlled_conjugation_box(op, n_controls, args));
      continue;
    }
    unsigned n_new_args = n_controls + n_args;
    qubit_vector_t new_args(n_new_args);
    for (unsigned i = 0; i < n_controls; i++) {
      new_args[i] = Qubit(i);
    }
    for (unsigned i = 0; i < n_args; i++) {
      new_args[n_controls + i] = Qubit(n_controls + args[i].index()[0]);
    }
    std::vector<Expr> params = op->get_params();
    switch (optype) {
      case OpType::noop:
        break;
      case OpType::X:
      case OpType::CX:
      case OpType::CCX:
      case OpType::CnX:
        c2.add_op<Qubit>(CNXTYPE(n_new_args), new_args);
        break;
      case OpType::Ry:
      case OpType::CRy:
      case OpType::CnRy:
        c2.add_op<Qubit>(CNRYTYPE(n_new_args), params, new_args);
        break;
      case OpType::Rx:
      case OpType::CRx:
      case OpType::CnRx:
        c2.add_op<Qubit>(CNRXTYPE(n_new_args), params, new_args);
        break;
      case OpType::Rz:
      case OpType::CRz:
      case OpType::CnRz:
        c2.add_op<Qubit>(CNRZTYPE(n_new_args), params, new_args);
        break;
      case OpType::Z:
      case OpType::CZ:
      case OpType::CnZ:
        c2.add_op<Qubit>(CNZTYPE(n_new_args), new_args);
        break;
      case OpType::Y:
      case OpType::CY:
      case OpType::CnY:
        c2.add_op<Qubit>(CNYTYPE(n_new_args), new_args);
        break;
      default: {
        std::vector<Expr> tk1_angles = as_gate_ptr(op)->get_tk1_angles();
        Expr theta = tk1_angles[1];
        Expr phi = tk1_angles[0] - 0.5;
        Expr lambda = tk1_angles[2] + 0.5;
        Expr t = tk1_angles[3] - 0.5 * (tk1_angles[0] + tk1_angles[2]);
        // Operation is U3(theta, phi, lambda) + phase t.
        // First absorb t in the overall phase.
        a += t;
        // Construct a multi-controlled U3, by extending the standard
        // CU3-to-CX decomposition.
        Qubit target = new_args[n_controls];
        Circuit cnu1 = CnU1(n_controls - 1, 0.5 * (lambda + phi));
        c2.append(cnu1);
        c2.add_op<Qubit>(OpType::U1, 0.5 * (lambda - phi), {target});
        c2.add_op<Qubit>(CNXTYPE(n_new_args), new_args);
        c2.add_op<Qubit>(
            OpType::U3, {-0.5 * theta, 0, -0.5 * (lambda + phi)}, {target});
        c2.add_op<Qubit>(CNXTYPE(n_new_args), new_args);
        c2.add_op<Qubit>(OpType::U3, {0.5 * theta, phi, 0}, {target});
      } break;
    }
  }

  // 3. Account for phase by appending a CnU1 to the control qubits.
  if (!equiv_0(a)) {
    Circuit cnu1 = CnU1(n_controls - 1, a);
    c2.append(cnu1);
  }

  c2.remove_noops();
  return c2;
}

// Return the target unitary given a Cn* gate where n >= 0
static Eigen::Matrix2cd get_target_op_matrix(const Op_ptr &op) {
  OpType optype = op->get_type();
  switch (optype) {
    case OpType::CX:
    case OpType::CCX:
    case OpType::CnX:
      return Gate(OpType::X, {}, 1).get_unitary();
    case OpType::CSX:
      return Gate(OpType::SX, {}, 1).get_unitary();
    case OpType::CSXdg:
      return Gate(OpType::SXdg, {}, 1).get_unitary();
    case OpType::CS:
      return Gate(OpType::S, {}, 1).get_unitary();
    case OpType::CSdg:
      return Gate(OpType::Sdg, {}, 1).get_unitary();
    case OpType::CV:
      return Gate(OpType::V, {}, 1).get_unitary();
    case OpType::CVdg:
      return Gate(OpType::Vdg, {}, 1).get_unitary();
    case OpType::CnRx:
    case OpType::CRx:
      return Gate(OpType::Rx, op->get_params(), 1).get_unitary();
    case OpType::CnRy:
    case OpType::CRy:
      return Gate(OpType::Ry, op->get_params(), 1).get_unitary();
    case OpType::CY:
    case OpType::CnY:
      return Gate(OpType::Y, {}, 1).get_unitary();
    case OpType::CnRz:
    case OpType::CRz:
      return Gate(OpType::Rz, op->get_params(), 1).get_unitary();
    case OpType::CZ:
    case OpType::CnZ:
      return Gate(OpType::Z, {}, 1).get_unitary();
    case OpType::CH:
      return Gate(OpType::H, {}, 1).get_unitary();
    case OpType::CU1:
      return Gate(OpType::U1, op->get_params(), 1).get_unitary();
    case OpType::CU3:
      return Gate(OpType::U3, op->get_params(), 1).get_unitary();
    default:
      if (!is_gate_type(optype) || op->n_qubits() != 1) {
        throw CircuitInvalidity(
            "Cannot get the target unitary of " + op->get_name());
      }
      return as_gate_ptr(op)->get_unitary();
  }
}

// A gate block containing Cn* gates that can be merged as a single CnU gate
// a block can also contain a single Barrier, which will be left in place
// TODO: conjugation boxes are accepted as well; however they don't fit the
// semantics. control_qubits and target_qubit don't mean anything for a
// conjugation box.
struct CnGateBlock {
  enum class MergeMode { append, prepend };
  CnGateBlock(const Command &command) {
    // Assumes the color of the target is not identity
    Op_ptr op = command.get_op_ptr();
    ops.push_back(op);
    unit_vector_t args = command.get_args();
    TKET_ASSERT(!args.empty());
    for (unsigned i = 0; i < args.size() - 1; i++) {
      control_qubits.insert(args[i].index()[0]);
    }
    target_qubit = args.back().index()[0];
    is_barrier = (op->get_type() == OpType::Barrier);
    is_conjugation_box = (op->get_type() == OpType::ConjugationBox);
    is_symmetric =
        (op->get_type() == OpType::CZ || op->get_type() == OpType::CnZ ||
         op->get_type() == OpType::CU1);
    color = (is_barrier || is_conjugation_box)
                ? std::nullopt
                : as_gate_ptr(op)->commuting_basis(args.size() - 1);
    if (color == Pauli::I) {
      throw std::invalid_argument(
          "CnGateBlock doesn't accept multi-controlled identity gate.");
    }
  }

  // Check whether commute with another CnGateBlock
  bool commutes_with(const CnGateBlock &other) {
    if (is_barrier || other.is_barrier || is_conjugation_box ||
        other.is_conjugation_box) {
      // they commute only if they have no args in common
      std::set<unsigned> common_args;
      std::set<unsigned> args = control_qubits;
      args.insert(target_qubit);
      std::set<unsigned> other_args = other.control_qubits;
      other_args.insert(other.target_qubit);
      std::set_intersection(
          args.begin(), args.end(), other_args.begin(), other_args.end(),
          std::inserter(common_args, common_args.begin()));
      return common_args.empty();
    }
    if (target_qubit == other.target_qubit) {
      return (color == other.color && color != std::nullopt);
    }
    if (control_qubits.contains(other.target_qubit) &&
        other.color != Pauli::Z) {
      return false;
    }
    if (other.control_qubits.contains(target_qubit) && color != Pauli::Z) {
      return false;
    }
    return true;
  }

  // Check whether can be merged with another CnGateBlock
  bool is_mergeable_with(const CnGateBlock &other) {
    if (is_barrier || other.is_barrier || is_conjugation_box ||
        other.is_conjugation_box) {
      return false;
    }
    // check if sizes match
    if (control_qubits.size() != other.control_qubits.size()) {
      return false;
    }
    // check if they act on the same set of qubits
    std::set<unsigned> args_a = control_qubits;
    args_a.insert(target_qubit);
    std::set<unsigned> args_b = other.control_qubits;
    args_b.insert(other.target_qubit);
    if (args_a != args_b) {
      return false;
    }
    // false if target don't match and none of them is symmetric
    if (target_qubit != other.target_qubit && !is_symmetric &&
        !other.is_symmetric) {
      return false;
    }
    return true;
  }

  // Merge with another CnGateBlock
  void merge(CnGateBlock &other, const MergeMode &mode) {
    if (mode == MergeMode::append) {
      ops.insert(ops.end(), other.ops.begin(), other.ops.end());
    } else {
      ops.insert(ops.begin(), other.ops.begin(), other.ops.end());
    }
    color = (color != other.color) ? std::nullopt : color;
    if (is_symmetric && !other.is_symmetric) {
      control_qubits = other.control_qubits;
      target_qubit = other.target_qubit;
      is_symmetric = false;
    }
    // empty the other CnGateBlock
    other.ops.clear();
  }

  Eigen::Matrix2cd get_target_unitary() const {
    Eigen::Matrix2cd m = Eigen::Matrix2cd::Identity();
    for (const Op_ptr &op : ops) {
      m = unitary_product2(get_target_op_matrix(op), m);
    }
    return m;
  }

  // ops in the block
  std::vector<Op_ptr> ops;
  // target qubit index
  unsigned target_qubit;
  // control indices
  std::set<unsigned> control_qubits;
  // whether the block is used as a barrier
  bool is_barrier;
  // whether the block contains a single ConjugationBox
  bool is_conjugation_box;
  // whether the target can act on any of its qubits
  bool is_symmetric;
  // color of the target qubit
  std::optional<Pauli> color;
};

// Construct a controlled version of a given circuit
// with the assumption that the circuit does not have symbols.
static Circuit with_controls_numerical(const Circuit &c, unsigned n_controls) {
  if (c.n_bits() != 0 || !c.is_simple()) {
    throw CircuitInvalidity("Only default qubit register allowed");
  }

  Circuit c1(c);
  // Replace wire swaps with SWAP gates
  c1.replace_all_implicit_wire_swaps();

  // Dispose of the trivial case
  if (n_controls == 0) {
    return c1;
  }
  // 1. Rebase to Cn* gates (n=0 for single qubit gates)
  VertexList bin;
  BGL_FORALL_VERTICES(v, c1.dag, DAG) {
    Op_ptr op = c1.get_Op_ptr_from_Vertex(v);
    OpType optype = op->get_type();
    if (is_gate_type(optype)) {
      if (is_projective_type(optype)) {
        throw CircuitInvalidity("Projective operations present");
      }
      if (is_single_qubit_type(optype) || is_controlled_gate_type(optype)) {
        continue;
      }
      Circuit replacement;
      if (optype == OpType::PhaseGadget) {
        replacement = phase_gadget(
            op->n_qubits(), op->get_params()[0], CXConfigType::Snake);
        if (replacement.n_gates() > 0) {
          TKET_ASSERT(
              replacement.n_gates() == 1 &&
              replacement.count_gates(OpType::ConjugationBox) == 1);
        }
      } else {
        replacement = with_CX(as_gate_ptr(op));
      }
      c1.substitute(replacement, v, Circuit::VertexDeletion::No);
      bin.push_back(v);
    } else if (is_box_type(optype) && optype != OpType::ConjugationBox) {
      throw CircuitInvalidity("Undecomposed boxes present");
    } else if (
        optype != OpType::Input && optype != OpType::Output &&
        optype != OpType::Barrier && optype != OpType::ConjugationBox) {
      throw CircuitInvalidity(
          "Cannot construct the controlled version of " + op->get_name());
    }
  }
  c1.remove_vertices(
      bin, Circuit::GraphRewiring::No, Circuit::VertexDeletion::Yes);

  // 2. try to partition the circuit into blocks of Cn* gates such that
  // the gates in each block can be merged into a single CnU gate
  std::vector<Command> commands = c1.get_commands();
  std::vector<CnGateBlock> blocks;

  Expr controlled_phase = c1.get_phase();

  for (const Command &cmd : commands) {
    // if the gate is an identity up to a phase, add it as a controlled phase
    std::optional<double> phase = std::nullopt;
    OpType optype = cmd.get_op_ptr()->get_type();
    if (optype != OpType::Barrier && optype != OpType::ConjugationBox) {
      phase = cmd.get_op_ptr()->is_identity();
    }
    if (phase != std::nullopt) {
      controlled_phase += phase.value();
    } else {
      blocks.push_back(CnGateBlock(cmd));
    }
  }

  // iterate the blocks from left to right
  for (unsigned i = 0; i + 1 < blocks.size(); i++) {
    CnGateBlock &b = blocks[i];
    if (b.ops.empty()) {
      continue;
    }
    // try to merge b to a block on the right
    for (unsigned j = i + 1; j < blocks.size(); j++) {
      CnGateBlock &candidate = blocks[j];
      if (candidate.ops.empty()) {
        continue;
      }
      if (b.is_mergeable_with(candidate)) {
        candidate.merge(b, CnGateBlock::MergeMode::prepend);
        break;
      }
      if (!b.commutes_with(candidate)) {
        break;
      }
    }
  }

  // iterate the blocks from right to left
  for (unsigned i = blocks.size(); i-- > 1;) {
    CnGateBlock &b = blocks[i];
    if (b.ops.empty()) {
      continue;
    }
    // try to merge b to a block on the left
    // iterate from i-1 to 0
    for (unsigned j = i; j-- > 0;) {
      CnGateBlock &candidate = blocks[j];
      if (candidate.ops.empty()) {
        continue;
      }
      if (b.is_mergeable_with(candidate)) {
        candidate.merge(b, CnGateBlock::MergeMode::append);
        break;
      }
      if (!b.commutes_with(candidate)) {
        break;
      }
    }
  }
  // 3. Add each block to c2 either as a CnX, CnZ, CnY, CnU decomposition
  // or an in-place Barrier
  Circuit c2(n_controls + c1.n_qubits());
  const static Eigen::Matrix2cd X = Gate(OpType::X, {}, 1).get_unitary();
  const static Eigen::Matrix2cd Y = Gate(OpType::Y, {}, 1).get_unitary();
  const static Eigen::Matrix2cd Z = Gate(OpType::Z, {}, 1).get_unitary();

  for (const CnGateBlock &b : blocks) {
    if (b.ops.empty()) {
      continue;
    }
    // Barriers are left in place
    if (b.is_barrier) {
      qubit_vector_t new_args;
      for (const unsigned i : b.control_qubits) {
        new_args.push_back(Qubit(i + n_controls));
      }
      new_args.push_back(Qubit(b.target_qubit + n_controls));
      TKET_ASSERT(b.ops.size() == 1);
      c2.add_op(b.ops[0], new_args);
      continue;
    }
    if (b.is_conjugation_box) {
      unit_vector_t args;
      for (const unsigned i : b.control_qubits) {
        args.push_back(Qubit(i));
      }
      args.push_back(Qubit(b.target_qubit));
      TKET_ASSERT(b.ops.size() == 1);
      c2.append(controlled_conjugation_box(b.ops[0], n_controls, args));
      continue;
    }
    // Computes the target unitary
    Eigen::Matrix2cd m = b.get_target_unitary();
    if (m.isApprox(Eigen::Matrix2cd::Identity(), EPS)) {
      continue;
    }
    std::function<qubit_vector_t()> get_args = [&]() {
      qubit_vector_t new_args;
      for (unsigned i = 0; i < n_controls; i++) {
        new_args.push_back(Qubit(i));
      }
      for (const unsigned i : b.control_qubits) {
        new_args.push_back(Qubit(i + n_controls));
      }
      new_args.push_back(Qubit(b.target_qubit + n_controls));
      return new_args;
    };
    if (m.isApprox(X, EPS)) {
      qubit_vector_t new_args = get_args();
      c2.add_op<Qubit>(CNXTYPE(new_args.size()), new_args);
      continue;
    }
    if (m.isApprox(Y, EPS)) {
      qubit_vector_t new_args = get_args();
      c2.add_op<Qubit>(CNYTYPE(new_args.size()), new_args);
      continue;
    }
    if (m.isApprox(Z, EPS)) {
      qubit_vector_t new_args = get_args();
      c2.add_op<Qubit>(CNZTYPE(new_args.size()), new_args);
      continue;
    }
    unit_map_t unit_map;
    for (unsigned i = 0; i < n_controls; i++) {
      unit_map.insert({Qubit(i), Qubit(i)});
    }
    unsigned control_index = n_controls;
    for (const unsigned i : b.control_qubits) {
      unit_map.insert({Qubit(control_index++), Qubit(i + n_controls)});
    }
    unit_map.insert({Qubit(control_index), Qubit(b.target_qubit + n_controls)});

    unsigned total_controls = b.control_qubits.size() + n_controls;

    Circuit replacement;

    // Check if the matrix is SU(2)
    if (std::abs(m.determinant() - 1.) < EPS) {
      // We have three functions that can decompose a multi-controlled SU(2).
      // The choice is based on the average number of CXs they produce for a
      // random n-controlled SU(2) gate.
      if (total_controls > 2 && total_controls < 5) {
        replacement = CircPool::CnU_gray_code_decomp(total_controls, m);
      } else if (total_controls >= 5 && total_controls < 9) {
        replacement = CircPool::CnU_linear_depth_decomp(total_controls, m);
      } else {
        // Compute the SU(2) angles from the TK1 angles
        std::vector<double> angles = tk1_angles_from_unitary(m);
        if (equiv_val(angles[3], 1., 2)) {
          // if the phase is odd, it can be absorbed into the first Rz rotation
          angles[0] = angles[0] + 2;
        } else {
          // because it's SU(2), the phase must be integers
          TKET_ASSERT(equiv_0(angles[3], 2));
        }
        // convert tk1 angles to zyz angles
        std::vector<double> zyz_angles = {
            angles[0] - 0.5, angles[1], angles[2] + 0.5};
        replacement = CircPool::CnSU2_linear_decomp(
            total_controls, zyz_angles[0], zyz_angles[1], zyz_angles[2]);
      }
    } else {
      // The gray code method produces less CXs when total_controls is 3 or 4
      if (total_controls == 3 || total_controls == 4) {
        replacement = CircPool::CnU_gray_code_decomp(total_controls, m);
      } else {
        replacement = CircPool::CnU_linear_depth_decomp(total_controls, m);
      }
    }
    c2.append_with_map(replacement, unit_map);
  }

  // 4. implement the controlled phase as a CnU1 gate
  if (!equiv_0(controlled_phase)) {
    Circuit cnu1_circ = CnU1(n_controls - 1, controlled_phase);
    c2.append(cnu1_circ);
  }
  return c2;
}

Circuit with_controls(const Circuit &c, unsigned n_controls) {
  if (c.is_symbolic()) {
    return with_controls_symbolic(c, n_controls);
  } else {
    return with_controls_numerical(c, n_controls);
  }
}

#undef CNXTYPE
#undef CNZTYPE
#undef CNYTYPE
#undef CNRYTYPE
#undef CNRXTYPE
#undef CNRZTYPE

std::tuple<Circuit, std::array<Expr, 3>, Circuit> normalise_TK2_angles(
    Expr a, Expr b, Expr c) {
  std::optional<double> a_eval = eval_expr_mod(a, 4);
  std::optional<double> b_eval = eval_expr_mod(b, 4);
  std::optional<double> c_eval = eval_expr_mod(c, 4);

  Circuit pre(2), post(2);

  // Add ot.dagger() at beginning and ot at end.
  auto conj = [&pre, &post](OpType ot) {
    Op_ptr op = get_op_ptr(ot);
    Op_ptr opdg = op->dagger();
    pre.add_op<unsigned>(opdg, {0});
    pre.add_op<unsigned>(opdg, {1});
    // These get undaggered at the end
    post.add_op<unsigned>(opdg, {0});
    post.add_op<unsigned>(opdg, {1});
  };

  // Step 1: For non-symbolic: a, b, c ∈ [0, 1] ∪ [3, 4].
  if (a_eval && *a_eval > 1. && *a_eval <= 3.) {
    a -= 2;
    *a_eval -= 2;
    pre.add_phase(1);
    *a_eval = fmodn(*a_eval, 4);
  }
  if (b_eval && *b_eval > 1. && *b_eval <= 3.) {
    b -= 2;
    *b_eval -= 2;
    pre.add_phase(1);
    *b_eval = fmodn(*b_eval, 4);
  }
  if (c_eval && *c_eval > 1. && *c_eval <= 3.) {
    c -= 2;
    *c_eval -= 2;
    pre.add_phase(1);
    *c_eval = fmodn(*c_eval, 4);
  }

  // Step 2: Make sure that symbolic expressions come before non-symbolics.
  if (a_eval && !b_eval) {
    // Swap XX and YY.
    conj(OpType::S);
    std::swap(a, b);
    std::swap(a_eval, b_eval);
  } else if (a_eval && !c_eval) {
    // Swap XX and ZZ.
    conj(OpType::H);
    std::swap(a, c);
    std::swap(a_eval, c_eval);
  }
  if (b_eval && !c_eval) {
    // Swap YY and ZZ.
    conj(OpType::V);
    std::swap(b, c);
    std::swap(b_eval, c_eval);
  }

  // Step 3: Order non-symbolic expressions in decreasing order.
  auto val_in_weyl = [](double r) {
    // Value of r once projected into Weyl chamber.
    return std::min(fmodn(r, 1), 1 - fmodn(r, 1));
  };
  if (a_eval && b_eval && val_in_weyl(*a_eval) < val_in_weyl(*b_eval)) {
    // Swap XX and YY.
    conj(OpType::S);
    std::swap(a, b);
    std::swap(a_eval, b_eval);
  }
  if (b_eval && c_eval && val_in_weyl(*b_eval) < val_in_weyl(*c_eval)) {
    // Swap YY and ZZ.
    conj(OpType::V);
    std::swap(b, c);
    std::swap(b_eval, c_eval);
  }
  if (a_eval && b_eval && val_in_weyl(*a_eval) < val_in_weyl(*b_eval)) {
    // Swap XX and YY.
    conj(OpType::S);
    std::swap(a, b);
    std::swap(a_eval, b_eval);
  }

  // Step 4: Project into Weyl chamber.
  if (a_eval && *a_eval > 1.) {
    a -= 3.;
    *a_eval -= 3;
    post.add_op<unsigned>(OpType::X, {0});
    post.add_op<unsigned>(OpType::X, {1});
    pre.add_phase(0.5);
  }
  if (b_eval && *b_eval > 1.) {
    b -= 3.;
    *b_eval -= 3;
    post.add_op<unsigned>(OpType::Y, {0});
    post.add_op<unsigned>(OpType::Y, {1});
    pre.add_phase(0.5);
  }
  if (c_eval && *c_eval > 1.) {
    c -= 3.;
    *c_eval -= 3;
    post.add_op<unsigned>(OpType::Z, {0});
    post.add_op<unsigned>(OpType::Z, {1});
    pre.add_phase(0.5);
  }
  if (a_eval && *a_eval > .5) {
    a = 1. - a;
    *a_eval = 1. - *a_eval;
    b = 1. - b;
    *b_eval = 1. - *b_eval;
    pre.add_op<unsigned>(OpType::Z, {0});
    post.add_op<unsigned>(OpType::Z, {1});
  }
  if (b_eval && (*b_eval > .5)) {
    b = 1 - b;
    *b_eval = 1. - *b_eval;
    c = 1 - c;
    *c_eval = 1. - *c_eval;
    pre.add_op<unsigned>(OpType::X, {0});
    post.add_op<unsigned>(OpType::X, {1});
  }
  if (c_eval && *c_eval > .5) {
    c -= 1;
    *c_eval -= 1.;
    post.add_op<unsigned>(OpType::Z, {0});
    post.add_op<unsigned>(OpType::Z, {1});
    pre.add_phase(-0.5);
  }
  // Calling post.dagger() is a cheeky way of reversing order of ops.
  return {pre, {a, b, c}, post.dagger()};
}

std::optional<double> is_TK2_SWAP(
    const Expr &alpha, const Expr &beta, const Expr &gamma) {
  std::optional<double> a = eval_expr_mod(alpha, 4);
  std::optional<double> b = eval_expr_mod(beta, 4);
  std::optional<double> c = eval_expr_mod(gamma, 4);
  if (a != std::nullopt && b != std::nullopt && c != std::nullopt &&
      approx_eq(*a, *b, 4) && approx_eq(*a, *c, 4)) {
    if (approx_eq(*a, 0.5, 4)) {
      return 1.75;
    } else if (approx_eq(*a, 1.5, 4)) {
      return 1.25;
    } else if (approx_eq(*a, 2.5, 4)) {
      return 0.75;
    } else if (approx_eq(*a, 3.5, 4)) {
      return 0.25;
    }
  }
  return std::nullopt;
}

}  // namespace tket
