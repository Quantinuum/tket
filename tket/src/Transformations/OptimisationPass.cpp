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

#include "tket/Transformations/OptimisationPass.hpp"

#include <stdexcept>

#include "tket/Circuit/CircPool.hpp"
#include "tket/Circuit/CircUtils.hpp"
#include "tket/Gate/GatePtr.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Transformations/BasicOptimisation.hpp"
#include "tket/Transformations/CliffordOptimisation.hpp"
#include "tket/Transformations/CliffordReductionPass.hpp"
#include "tket/Transformations/Combinator.hpp"
#include "tket/Transformations/Decomposition.hpp"
#include "tket/Transformations/PhaseOptimisation.hpp"
#include "tket/Transformations/Rebase.hpp"
#include "tket/Transformations/ThreeQubitSquash.hpp"
#include "tket/Transformations/Transform.hpp"

namespace tket {

namespace Transforms {

Transform peephole_optimise_2q(bool allow_swaps) {
  return (
      synthesise_tket() >> two_qubit_squash(allow_swaps) >>
      decompose_multi_qubits_CX() >> clifford_simp(allow_swaps) >>
      synthesise_tket());
}

Transform full_peephole_optimise(bool allow_swaps, OpType target_2qb_gate) {
  switch (target_2qb_gate) {
    case OpType::CX:
      return (
          remove_redundancies() >> synthesise_tket() >>
          two_qubit_squash(false) >> clifford_simp(allow_swaps) >>
          synthesise_tket() >> two_qubit_squash(allow_swaps) >>
          three_qubit_squash() >> clifford_simp(allow_swaps) >>
          synthesise_tket());
    case OpType::TK2:
      return (
          remove_redundancies() >> synthesise_tk() >>
          two_qubit_squash(OpType::TK2, 1., allow_swaps) >>
          clifford_simp(allow_swaps, OpType::TK2) >>
          two_qubit_squash(OpType::TK2, 1., allow_swaps) >> synthesise_tk() >>
          three_qubit_squash(OpType::TK2) >>
          clifford_simp(allow_swaps, OpType::TK2) >>
          two_qubit_squash(OpType::TK2, 1., allow_swaps) >> synthesise_tk());
    default:
      throw std::invalid_argument("Invalid target 2-qubit gate");
  }
}

Transform clifford_simp(bool allow_swaps, OpType target_2qb_gate) {
  if (target_2qb_gate != OpType::CX && target_2qb_gate != OpType::TK2) {
    throw std::invalid_argument("Invalid target 2-qubit gate");
  }
  return decompose_cliffords_std(target_2qb_gate == OpType::CX) >>
         clifford_reduction(allow_swaps) >>
         (target_2qb_gate == OpType::CX ? decompose_multi_qubits_CX()
                                        : decompose_multi_qubits_TK2()) >>
         singleq_clifford_sweep() >> squash_1qb_to_tk1();
}

Transform synthesise_tk() {
  Transform seq = commute_through_multis() >> remove_redundancies();
  Transform rep = repeat(seq);
  Transform synth = decompose_multi_qubits_TK2() >> remove_redundancies() >>
                    rep >> squash_1qb_to_tk1();
  Transform small_part = remove_redundancies() >> rep >> squash_1qb_to_tk1();
  Transform repeat_synth = repeat_with_metric(
      small_part, [](const Circuit &circ) { return circ.n_vertices(); });
  return synth >> repeat_synth >> rebase_TK() >> remove_redundancies();
}

Transform synthesise_tket() {
  Transform seq = commute_through_multis() >> remove_redundancies();
  Transform rep = repeat(seq);
  Transform synth = decompose_multi_qubits_CX() >> remove_redundancies() >>
                    rep >> squash_1qb_to_tk1();
  Transform small_part = remove_redundancies() >> rep >> squash_1qb_to_tk1();
  Transform repeat_synth = repeat_with_metric(
      small_part, [](const Circuit &circ) { return circ.n_vertices(); });
  return synth >> repeat_synth >> rebase_tket() >> remove_redundancies();
}

static Transform CXs_from_phase_gadgets(CXConfigType cx_config) {
  return Transform([=](Circuit &circ) {
    bool success = false;
    VertexList bin;
    auto [i, end] = boost::vertices(circ.dag);
    for (auto next = i; i != end; i = next) {
      ++next;
      Vertex a = *i;
      Op_ptr op = circ.get_Op_ptr_from_Vertex(a);
      if (op->get_type() == OpType::PhaseGadget) {
        unsigned n_qubits = op->n_qubits();
        Circuit replacement =
            phase_gadget(n_qubits, op->get_params()[0], cx_config);
        Subcircuit sub = circ.singleton_subcircuit(a);
        circ.substitute(replacement, sub, Circuit::VertexDeletion::Yes);
        success = true;
      }
    }
    circ.decompose_boxes_recursively();
    return success;
  });
}

Transform optimise_via_PhaseGadget(CXConfigType cx_config) {
  return rebase_tket() >> decompose_PhaseGadgets() >> smash_CX_PhaseGadgets() >>
         align_PhaseGadgets() >> CXs_from_phase_gadgets(cx_config) >>
         synthesise_tket();
}

// TODO: Make the XXPhase gates combine
Transform synthesise_UMD() {
  return Transform([](Circuit &circ) {
           bool success = (synthesise_tket() >> decompose_ZX() >>
                           decompose_MolmerSorensen() >> squash_1qb_to_tk1())
                              .apply(circ);
           VertexList bin;
           BGL_FORALL_VERTICES(v, circ.dag, DAG) {
             const Op_ptr op_ptr = circ.get_Op_ptr_from_Vertex(v);
             OpType type = op_ptr->get_type();
             if (type == OpType::TK1) {
               std::vector<Expr> tk1_angles =
                   as_gate_ptr(op_ptr)->get_tk1_angles();
               Circuit in_circ = CircPool::tk1_to_PhasedXRz(
                   tk1_angles[0], tk1_angles[1], tk1_angles[2]);
               remove_redundancies().apply(in_circ);
               Subcircuit sub = circ.singleton_subcircuit(v);
               bin.push_back(v);
               circ.substitute(in_circ, sub, Circuit::VertexDeletion::No);
               circ.add_phase(tk1_angles[3]);
               success = true;
             }
           }
           circ.remove_vertices(
               bin, Circuit::GraphRewiring::No, Circuit::VertexDeletion::Yes);
           return success;
         }) >>
         rebase_UMD() >> remove_redundancies();
}

}  // namespace Transforms

}  // namespace tket
