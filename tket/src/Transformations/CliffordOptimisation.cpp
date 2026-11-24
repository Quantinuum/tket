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

#include "tket/Transformations/CliffordOptimisation.hpp"

#include <boost/bimap.hpp>
#include <vector>

#include "tket/Circuit/CircPool.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Circuit/DAGDefs.hpp"
#include "tket/Clifford/UnitaryTableau.hpp"
#include "tket/Converters/Converters.hpp"
#include "tket/Diagonalisation/Diagonalisation.hpp"
#include "tket/Ops/ClassicalOps.hpp"
#include "tket/Transformations/BasicOptimisation.hpp"
#include "tket/Transformations/Decomposition.hpp"
#include "tket/Transformations/Transform.hpp"
#include "tket/Utils/PauliTensor.hpp"
#include "tket/Utils/UnitID.hpp"

namespace tket {

namespace Transforms {

static bool multiq_clifford_match(Circuit &circ, bool allow_swaps);
static bool copy_pi_through_CX_method(Circuit &circ);

Transform multiq_clifford_replacement(bool allow_swaps) {
  return Transform([allow_swaps](Circuit &circ) {
    return multiq_clifford_match(circ, allow_swaps);
  });
}

static bool multiq_clifford_match(Circuit &circ, bool allow_swaps) {
  bool success = false;
  // map from vertex/port to qubit number
  std::map<Vertex, unit_set_t> v_to_qb = circ.vertex_unit_map();
  // map from vertex to its depth from slices/reverse_slices
  std::map<Vertex, unsigned> v_to_depth = circ.vertex_depth_map();
  std::map<Vertex, unsigned> v_to_rev_depth = circ.vertex_rev_depth_map();
  // Analysis complete, we can now go through and actually do the transformation
  VertexList bin;
  BGL_FORALL_VERTICES(v, circ.dag, DAG) {
    if (circ.get_OpType_from_Vertex(v) == OpType::CX &&
        circ.n_out_edges(v) == 2) {
      // Walk to next CX on the same pair
      unit_set_t cx_units = v_to_qb.at(v);
      qubit_vector_t qbs(cx_units.begin(), cx_units.end());
      QPathDetailed path0;
      QPathDetailed path1;
      bool found_cnot = false;
      Edge e0 = circ.get_nth_out_edge(v, 0);
      Vertex v0 = circ.target(e0);
      port_t port = circ.get_target_port(e0);
      while (true) {
        OpType v_type = circ.get_OpType_from_Vertex(v0);
        if (is_final_q_type(v_type)) {
          break;
        } else if (v_type == OpType::CX && v_to_qb.at(v0) == cx_units) {
          found_cnot = true;
          break;
        }
        path0.push_back({v0, port});
        e0 = circ.get_next_edge(v0, e0);
        v0 = circ.target(e0);
        port = circ.get_target_port(e0);
      }
      if (!found_cnot) {
        continue;
      }
      Edge e1 = circ.get_nth_out_edge(v, 1);
      Vertex v1 = circ.target(e1);
      while (v1 != v0) {
        path1.push_back({v1, circ.get_target_port(e1)});
        e1 = circ.get_next_edge(v1, e1);
        v1 = circ.target(e1);
      }
      unsigned path0_length = path0.size();
      unsigned path1_length = path1.size();
      unsigned front0_index = 0;
      unsigned front1_index = 0;
      while (front0_index < path0_length &&
             circ.commutes_with_basis(
                 path0[front0_index].first, Pauli::Z,
                 path0[front0_index].second)) {
        front0_index++;
      }
      while (front1_index < path1_length &&
             circ.commutes_with_basis(
                 path1[front1_index].first, Pauli::X,
                 path1[front1_index].second)) {
        front1_index++;
      }
      if (port == 0) {
        // CXs have same orientation
        // We walk back from the end of the path
        // unsigned is used as this is semantically an index and the check for
        // -1 functions the same as if it were signed
        unsigned back0_index = path0_length - 1;
        while (back0_index != front0_index && back0_index != (unsigned)-1 &&
               circ.commutes_with_basis(
                   path0[back0_index].first, Pauli::Z,
                   path0[back0_index].second)) {
          back0_index--;
        }
        unsigned back1_index = path1_length - 1;
        while (back1_index != front1_index && back1_index != (unsigned)-1 &&
               circ.commutes_with_basis(
                   path1[back1_index].first, Pauli::X,
                   path1[back1_index].second)) {
          back1_index--;
        }
        // Check for valid causal ordering
        Vertex front0pre =
            (front0_index == 0) ? v : path0[front0_index - 1].first;
        Vertex front0post =
            (front0_index == path0_length) ? v0 : path0[front0_index].first;
        Vertex front1pre =
            (front1_index == 0) ? v : path1[front1_index - 1].first;
        Vertex front1post =
            (front1_index == path1_length) ? v0 : path1[front1_index].first;
        Vertex back0pre =
            (back0_index == (unsigned)-1) ? v : path0[back0_index].first;
        Vertex back0post = (back0_index == path0_length - 1)
                               ? v0
                               : path0[back0_index + 1].first;
        Vertex back1pre =
            (back1_index == (unsigned)-1) ? v : path1[back1_index].first;
        Vertex back1post = (back1_index == path1_length - 1)
                               ? v0
                               : path1[back1_index + 1].first;
        if (circ.in_causal_order(
                front1pre, front0post, true, v_to_depth, v_to_qb))
          continue;
        if (circ.in_causal_order(
                front0pre, front1post, true, v_to_depth, v_to_qb))
          continue;
        if (circ.in_causal_order(
                back1post, back0pre, false, v_to_rev_depth, v_to_qb))
          continue;
        if (circ.in_causal_order(
                back0post, back1pre, false, v_to_rev_depth, v_to_qb))
          continue;
        // Identify which pattern to replace
        bool v_on_q0 = false;
        if (front0_index != path0_length) {
          if (circ.get_OpType_from_Vertex(path0[front0_index].first) !=
              OpType::V) {
            // The only non-empty pattern we can recognise is from a V
            continue;
          }
          v_on_q0 = true;
          if (back0_index != front0_index) {
            // There are some ops after the V that we cannot commute past the CX
            continue;
          }
        }
        bool s_on_q1 = false;
        if (front1_index != path1_length) {
          if (circ.get_OpType_from_Vertex(path1[front1_index].first) !=
              OpType::S) {
            // The only non-empty pattern we can recognise is from a S
            continue;
          }
          s_on_q1 = true;
          if (back1_index == front1_index + 2 &&
              circ.get_OpType_from_Vertex(path1[front1_index + 1].first) ==
                  OpType::V &&
              circ.get_OpType_from_Vertex(path1[front1_index + 2].first) ==
                  OpType::S) {
            /* --C--  ...  --C--      --C--  ...  --C--  */
            /*   |           |    =>    |           |    */
            /* --X--S--V--S--X--      --X--V--S--V--X--  */
            circ.dag[path1[front1_index].first] = {get_op_ptr(OpType::V)};
            circ.dag[path1[front1_index + 1].first] = {get_op_ptr(OpType::S)};
            circ.dag[path1[front1_index + 2].first] = {get_op_ptr(OpType::V)};
            circ.add_phase(0.25);
            front1_index++;
            back1_index--;
          } else if (back1_index != front1_index) {
            // There are some ops after the S that we cannot commute past the CX
            continue;
          }
        }
        if (!allow_swaps && v_on_q0 && s_on_q1) {
          continue;
        }
        success = true;
        // Always remove the CX pair, saving the new edges from the second
        Edge next_e0 = circ.get_nth_out_edge(v0, 0);
        Edge next_e1 = circ.get_nth_out_edge(v0, 1);
        Vertex next_v0 = circ.target(next_e0);
        Vertex next_v1 = circ.target(next_e1);
        port_t next_p0 = circ.get_target_port(next_e0);
        port_t next_p1 = circ.get_target_port(next_e1);
        bin.push_back(v);
        bin.push_back(v0);
        circ.remove_vertex(
            v, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
        circ.remove_vertex(
            v0, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
        Edge default_h0 = circ.get_nth_in_edge(next_v0, next_p0);
        Edge default_h1 = circ.get_nth_in_edge(next_v1, next_p1);

        Subcircuit sub;
        const Circuit *replacement;
        Qubit q0, q1;
        if (v_on_q0) {
          Vertex v_gate = path0[front0_index].first;
          q0 = Qubit(*v_to_qb.at(v_gate).begin());
          q1 = qbs.at(0);
          if (q0 == q1) q1 = qbs.at(1);
          if (s_on_q1) {
            /*  --C--V--C--      --Z--S--V--X--S--\ /--  */
            /*    |     |    =>             |      \     */
            /*  --X--S--X--      --X--V--S--C--V--/ \--  */
            Vertex s_gate = path1[front1_index].first;
            bin.push_back(v_gate);
            bin.push_back(s_gate);
            sub = {
                {circ.get_nth_in_edge(v_gate, 0),
                 circ.get_nth_in_edge(s_gate, 0)},
                {circ.get_nth_out_edge(v_gate, 0),
                 circ.get_nth_out_edge(s_gate, 0)},
                {},
                {v_gate, s_gate}};
            replacement = &CircPool::CX_VS_CX_reduced();
          } else {
            /*  --C--V--C--      --X--V--S--V--C--V--S--  */
            /*    |     |    =>                |          */
            /*  --X-----X--      --V-----------X--------  */
            bin.push_back(v_gate);
            sub = {
                {circ.get_nth_in_edge(v_gate, 0), default_h1},
                {circ.get_nth_out_edge(v_gate, 0), default_h1},
                {},
                {v_gate}};
            replacement = &CircPool::CX_V_CX_reduced();
          }
        } else if (s_on_q1) {
          /* --C-----C--      --S-----------C--------  */
          /*   |     |    =>                |          */
          /* --X--S--X--      --Z--S--V--S--X--S--V--  */
          Vertex s_gate = path1[front1_index].first;
          q1 = Qubit(*v_to_qb.at(s_gate).begin());
          q0 = qbs.at(0);
          if (q0 == q1) q0 = qbs.at(1);
          bin.push_back(s_gate);
          sub = {
              {default_h0, circ.get_nth_in_edge(s_gate, 0)},
              {default_h0, circ.get_nth_out_edge(s_gate, 0)},
              {},
              {s_gate}};
          replacement = &CircPool::CX_S_CX_reduced();
        } else {
          /* --C--C--      ----  */
          /*   |  |    =>        */
          /* --X--X--      ----  */
          continue;
        }
        Vertex b0 = circ.source(sub.in_hole[0]);
        port_t p0 = circ.get_source_port(sub.in_hole[0]);
        Vertex b1 = circ.source(sub.in_hole[1]);
        port_t p1 = circ.get_source_port(sub.in_hole[1]);
        Vertex a0 = circ.target(*sub.out_hole[0]);
        Vertex a1 = circ.target(*sub.out_hole[1]);
        circ.substitute(*replacement, sub, Circuit::VertexDeletion::No);
        // Scan through hole and add vertices to v_to_qb
        // and give estimates of depth and rev_depth
        unsigned new_depth = std::min(v_to_depth[a0], v_to_depth[a1]);
        unsigned new_rev_depth =
            std::min(v_to_rev_depth[b0], v_to_rev_depth[b1]);
        Edge ei0 = circ.get_nth_out_edge(b0, p0);
        Vertex vi0 = circ.target(ei0);
        while (vi0 != a0) {
          v_to_qb.insert({vi0, {q0}});
          v_to_depth[vi0] = new_depth;
          v_to_rev_depth[vi0] = new_rev_depth;
          ei0 = circ.get_next_edge(vi0, ei0);
          vi0 = circ.target(ei0);
        }
        Edge ei1 = circ.get_nth_out_edge(b1, p1);
        Vertex vi1 = circ.target(ei1);
        while (vi1 != a1) {
          if (v_to_qb.find(vi1) == v_to_qb.end())
            v_to_qb.insert({vi1, {q1}});
          else
            v_to_qb.at(vi1).insert(q1);
          v_to_depth[vi1] = new_depth;
          v_to_rev_depth[vi1] = new_rev_depth;
          ei1 = circ.get_next_edge(vi1, ei1);
          vi1 = circ.target(ei1);
        }
      } else {
        // CXs have different orientation
        unsigned back0_index = path0_length - 1;
        while (back0_index >= front0_index && back0_index != (unsigned)-1 &&
               circ.commutes_with_basis(
                   path0[back0_index].first, Pauli::X,
                   path0[back0_index].second)) {
          back0_index--;
        }
        unsigned back1_index = path1_length - 1;
        while (back1_index >= front1_index && back1_index != (unsigned)-1 &&
               circ.commutes_with_basis(
                   path1[back1_index].first, Pauli::Z,
                   path1[back1_index].second)) {
          back1_index--;
        }
        Vertex front0pre =
            (front0_index == 0) ? v : path0[front0_index - 1].first;
        Vertex front0post =
            (front0_index == path0_length) ? v0 : path0[front0_index].first;
        Vertex front1pre =
            (front1_index == 0) ? v : path1[front1_index - 1].first;
        Vertex front1post =
            (front1_index == path1_length) ? v0 : path1[front1_index].first;
        Vertex back0pre =
            (back0_index == (unsigned)-1) ? v : path0[back0_index].first;
        Vertex back0post = (back0_index == path0_length - 1)
                               ? v0
                               : path0[back0_index + 1].first;
        Vertex back1pre =
            (back1_index == (unsigned)-1) ? v : path1[back1_index].first;
        Vertex back1post = (back1_index == path1_length - 1)
                               ? v0
                               : path1[back1_index + 1].first;
        if (circ.in_causal_order(
                front1pre, front0post, true, v_to_depth, v_to_qb))
          continue;
        if (circ.in_causal_order(
                front0pre, front1post, true, v_to_depth, v_to_qb))
          continue;
        if (circ.in_causal_order(
                back1post, back0pre, false, v_to_rev_depth, v_to_qb))
          continue;
        if (circ.in_causal_order(
                back0post, back1pre, false, v_to_rev_depth, v_to_qb))
          continue;
        // Identify which pattern
        bool vs_on_q0 = false;
        if (back0_index >= front0_index && back0_index != (unsigned)-1) {
          if (!(back0_index == front0_index + 1 &&
                circ.get_OpType_from_Vertex(path0[front0_index].first) ==
                    OpType::V &&
                circ.get_OpType_from_Vertex(path0[back0_index].first) ==
                    OpType::S)) {
            // The only non-empty pattern we can recognise is from VS
            continue;
          }
          vs_on_q0 = true;
        }
        bool sv_on_q1 = false;
        if (back1_index >= front1_index && back1_index != (unsigned)-1) {
          if (!(back1_index == front1_index + 1 &&
                circ.get_OpType_from_Vertex(path1[front1_index].first) ==
                    OpType::S &&
                circ.get_OpType_from_Vertex(path1[back1_index].first) ==
                    OpType::V)) {
            // The only non-empty pattern we can recognise is from SV
            continue;
          }
          sv_on_q1 = true;
        }
        if (!allow_swaps && !vs_on_q0 && !sv_on_q1) {
          continue;
        }
        success = true;
        // Always remove the CX pair, saving the edges to insert the replacement
        // circuit
        Vertex before0, before1, after0, after1;
        port_t pb0, pb1, pa0, pa1;
        if (front0_index > 0) {
          before0 = path0[front0_index - 1].first;
          pb0 = path0[front0_index - 1].second;
        } else {
          Edge pre0 = circ.get_nth_in_edge(v, 0);
          before0 = circ.source(pre0);
          pb0 = circ.get_source_port(pre0);
        }
        if (front1_index > 0) {
          before1 = path1[front1_index - 1].first;
          pb1 = path1[front1_index - 1].second;
        } else {
          Edge pre1 = circ.get_nth_in_edge(v, 1);
          before1 = circ.source(pre1);
          pb1 = circ.get_source_port(pre1);
        }
        if (back0_index + 1 < path0_length) {
          after0 = path0[back0_index + 1].first;
          pa0 = path0[back0_index + 1].second;
        } else {
          Edge post0 = circ.get_nth_out_edge(v0, 1);
          after0 = circ.target(post0);
          pa0 = circ.get_target_port(post0);
        }
        if (back1_index + 1 < path1_length) {
          after1 = path1[back1_index + 1].first;
          pa1 = path1[back1_index + 1].second;
        } else {
          Edge post1 = circ.get_nth_out_edge(v0, 0);
          after1 = circ.target(post1);
          pa1 = circ.get_target_port(post1);
        }
        bin.push_back(v);
        bin.push_back(v0);
        circ.remove_vertex(
            v, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
        circ.remove_vertex(
            v0, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
        VertexSet to_remove;
        const Circuit *replacement;
        Qubit q0, q1;
        if (vs_on_q0) {
          Vertex v_gate = path0[front0_index].first;
          q0 = Qubit(*v_to_qb.at(v_gate).begin());
          q1 = qbs.at(0);
          if (q0 == q1) q1 = qbs.at(1);
          if (sv_on_q1) {
            /* --C--V--S--X--      --V--S--  */
            /*   |        |    =>            */
            /* --X--S--V--C--      --S--V--  */
            continue;
          } else {
            /* --C--V--S--X--        --S-----C--V--S-- */
            /*   |        |     =>           |         */
            /* --X--------C--        --Z--S--X--S----- */
            replacement = &CircPool::CX_V_S_XC_reduced();
          }
          bin.push_back(path0[front0_index].first);
          bin.push_back(path0[back0_index].first);
          to_remove.insert(path0[front0_index].first);
          to_remove.insert(path0[back0_index].first);
        } else if (sv_on_q1) {
          /* --C--------X--      --X--V--C--V----- */
          /*   |        |    =>          |         */
          /* --X--S--V--C--      --V-----X--S--V-- */
          Vertex s_gate = path1[front1_index].first;
          q1 = Qubit(*v_to_qb.at(s_gate).begin());
          q0 = qbs.at(0);
          if (q0 == q1) q0 = qbs.at(1);
          replacement = &CircPool::CX_S_V_XC_reduced();
          bin.push_back(path1[front1_index].first);
          bin.push_back(path1[back1_index].first);
          to_remove.insert(path1[front1_index].first);
          to_remove.insert(path1[back1_index].first);
        } else {
          /* --C--X--      --X--\ /-- */
          /*   |  |    =>    |   \    */
          /* --X--C--      --C--/ \-- */
          q0 = qbs.at(0);
          q1 = qbs.at(1);
          replacement = &CircPool::CX_XC_reduced();
        }
        Subcircuit sub = {
            {circ.get_nth_out_edge(before0, pb0),
             circ.get_nth_out_edge(before1, pb1)},
            {circ.get_nth_in_edge(after0, pa0),
             circ.get_nth_in_edge(after1, pa1)},
            {},
            to_remove};
        Vertex b0 = circ.source(sub.in_hole[0]);
        port_t p0 = circ.get_source_port(sub.in_hole[0]);
        Vertex b1 = circ.source(sub.in_hole[1]);
        port_t p1 = circ.get_source_port(sub.in_hole[1]);
        Vertex a0 = circ.target(*sub.out_hole[0]);
        Vertex a1 = circ.target(*sub.out_hole[1]);
        circ.substitute(*replacement, sub, Circuit::VertexDeletion::No);
        // Scan through hole and add vertices to v_to_qb
        unsigned new_depth = std::min(v_to_depth[a0], v_to_depth[a1]);
        unsigned new_rev_depth =
            std::min(v_to_rev_depth[b0], v_to_rev_depth[b1]);
        Edge ei0 = circ.get_nth_out_edge(b0, p0);
        Vertex vi0 = circ.target(ei0);
        while (vi0 != a0) {
          v_to_qb.insert({vi0, {q0}});
          v_to_depth[vi0] = new_depth;
          v_to_rev_depth[vi0] = new_rev_depth;
          ei0 = circ.get_next_edge(vi0, ei0);
          vi0 = circ.target(ei0);
        }
        Edge ei1 = circ.get_nth_out_edge(b1, p1);
        Vertex vi1 = circ.target(ei1);
        while (vi1 != a1) {
          if (v_to_qb.find(vi1) == v_to_qb.end())
            v_to_qb.insert({vi1, {q1}});
          else
            v_to_qb.at(vi1).insert(q1);
          v_to_depth[vi1] = new_depth;
          v_to_rev_depth[vi1] = new_rev_depth;
          ei1 = circ.get_next_edge(vi1, ei1);
          vi1 = circ.target(ei1);
        }
      }
    }
  }
  if (allow_swaps) {
    success = circ.replace_SWAPs() || success;
  }
  circ.remove_vertices(
      bin, Circuit::GraphRewiring::No, Circuit::VertexDeletion::Yes);
  return success;
}

Transform copy_pi_through_CX() { return Transform(copy_pi_through_CX_method); }

// TODO:: Copy classical-controls and any controls from CX
static bool copy_pi_through_CX_method(Circuit &circ) {
  bool success = false;
  VertexList bin;
  BGL_FORALL_VERTICES(vert, circ.dag, DAG) {
    if (circ.get_OpType_from_Vertex(vert) == OpType::CX &&
        circ.n_out_edges(vert) == 2) {
      Edge e0 = circ.get_nth_out_edge(vert, 0);
      Vertex v0 = circ.target(e0);
      if (circ.get_OpType_from_Vertex(v0) == OpType::X) {
        success = true;
        Edge afterX = circ.get_next_edge(v0, e0);
        Edge after1 = circ.get_nth_out_edge(vert, 1);
        Vertex v_after1 = circ.target(after1);
        port_t pa1 = circ.get_target_port(after1);
        bin.push_back(vert);
        circ.remove_vertex(
            vert, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
        after1 = circ.get_nth_in_edge(v_after1, pa1);
        Subcircuit sub = {{afterX, after1}, {afterX, after1}, {}, {}};
        circ.substitute(CircPool::X1_CX(), sub, Circuit::VertexDeletion::No);
        continue;
      }
      Edge e1 = circ.get_nth_out_edge(vert, 1);
      Vertex v1 = circ.target(e1);
      if (circ.get_OpType_from_Vertex(v1) == OpType::Z) {
        success = true;
        Edge afterZ = circ.get_next_edge(v1, e1);
        Edge after0 = circ.get_nth_out_edge(vert, 0);
        Vertex v_after0 = circ.target(after0);
        port_t pa0 = circ.get_target_port(after0);
        bin.push_back(vert);
        circ.remove_vertex(
            vert, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
        after0 = circ.get_nth_in_edge(v_after0, pa0);
        Subcircuit sub = {{after0, afterZ}, {after0, afterZ}, {}, {}};
        circ.substitute(CircPool::Z0_CX(), sub, Circuit::VertexDeletion::No);
      }
    }
  }
  circ.remove_vertices(
      bin, Circuit::GraphRewiring::No, Circuit::VertexDeletion::Yes);
  return success;
}

// TODO:: Detect classical controls and stop simplification
static bool singleq_clifford_from_edge(
    Circuit &circ, Edge &e, VertexList &bin) {
  VertexSet single_vs;
  Edge ei = e;
  Vertex vi = circ.target(ei);
  // stuff to check if it is already in standard form
  // 6 = not consumed anything, 5=Z, 4=X, 3=first S, 2=V, 1=second S, 0=other
  unsigned cliff_last = 6;
  while (circ.detect_singleq_unitary_op(vi)) {
    // i.e. we have a single qubit unitary
    single_vs.insert(vi);

    // keep track of clifford standard form
    switch (circ.get_OpType_from_Vertex(vi)) {
      case OpType::Z: {
        cliff_last = (cliff_last > 5) ? 5 : 0;
        break;
      }
      case OpType::X: {
        cliff_last = (cliff_last > 4) ? 4 : 0;
        break;
      }
      case OpType::S: {
        if (cliff_last > 3)
          cliff_last = 3;
        else if (cliff_last == 2)
          cliff_last = 1;
        else
          cliff_last = 0;
        break;
      }
      case OpType::V: {
        cliff_last = (cliff_last > 2) ? 2 : 0;
        break;
      }
      default: {
        cliff_last = 0;
        break;
      }
    }

    ei = circ.get_next_edge(vi, ei);
    vi = circ.target(ei);
  }
  // if the sequence is not in the standard form, replace it
  if (cliff_last == 0) {
    Subcircuit s = {{e}, {ei}, {}, single_vs};
    Circuit sub = circ.subcircuit(s);
    bool reduced = (decompose_single_qubits_TK1() >> squash_1qb_to_tk1() >>
                    decompose_cliffords_std())
                       .apply(sub);
    if (reduced) {
      circ.substitute(sub, s, Circuit::VertexDeletion::No);
      bin.insert(bin.end(), single_vs.begin(), single_vs.end());
      return true;
    }
  }
  return false;
}

Transform singleq_clifford_sweep() {
  return Transform([](Circuit &circ) {
    bool success = false;
    VertexList bin;
    std::vector<Vertex> vertices = circ.vertices_in_order();
    for (auto it = vertices.crbegin(); it != vertices.crend(); ++it) {
      const Vertex &v = *it;
      OpType v_type = circ.get_OpType_from_Vertex(v);
      if (v_type != OpType::Conditional &&
          circ.n_out_edges_of_type(v, EdgeType::Quantum) == 2) {
        for (port_t p = 0; p < 2; p++) {
          // put both subsequent single qubit sequences into normal form
          // stuff for obtaining next single qubit subcircuit
          Edge e = circ.get_nth_out_edge(v, p);
          success = singleq_clifford_from_edge(circ, e, bin) || success;
        }
        if (v_type == OpType::CX) {
          // commute Z, X and whatever else we can through
          Edge e0 = circ.get_nth_out_edge(v, 0);
          Vertex v0 = circ.target(e0);

          if (circ.get_OpType_from_Vertex(v0) == OpType::Z) {
            circ.remove_vertex(
                v0, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
            Edge in_e = circ.get_nth_in_edge(v, 0);
            circ.rewire(v0, {in_e}, {EdgeType::Quantum});
            success = true;
            e0 = circ.get_nth_out_edge(v, 0);
            v0 = circ.target(e0);
          }
          if (circ.get_OpType_from_Vertex(v0) == OpType::X) {
            circ.remove_vertex(
                v0, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
            Edge in_e = circ.get_nth_in_edge(v, 0);
            circ.rewire(v0, {in_e}, {EdgeType::Quantum});
            Vertex new_v = circ.add_vertex(OpType::X);
            in_e = circ.get_nth_in_edge(v, 1);
            circ.rewire(new_v, {in_e}, {EdgeType::Quantum});
            success = true;
            e0 = circ.get_nth_out_edge(v, 0);
            v0 = circ.target(e0);
          }
          if (circ.get_OpType_from_Vertex(v0) == OpType::S) {
            circ.remove_vertex(
                v0, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
            Edge in_e = circ.get_nth_in_edge(v, 0);
            circ.rewire(v0, {in_e}, {EdgeType::Quantum});
            success = true;
          }
          Edge e1 = circ.get_nth_out_edge(v, 1);
          Vertex v1 = circ.target(e1);
          if (circ.get_OpType_from_Vertex(v1) == OpType::Z) {
            circ.remove_vertex(
                v1, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
            Edge in_e = circ.get_nth_in_edge(v, 1);
            circ.rewire(v1, {in_e}, {EdgeType::Quantum});
            Vertex new_v = circ.add_vertex(OpType::Z);
            in_e = circ.get_nth_in_edge(v, 0);
            circ.rewire(new_v, {in_e}, {EdgeType::Quantum});
            success = true;
            e1 = circ.get_nth_out_edge(v, 1);
            v1 = circ.target(e1);
          }
          if (circ.get_OpType_from_Vertex(v1) == OpType::X) {
            circ.remove_vertex(
                v1, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
            Edge in_e = circ.get_nth_in_edge(v, 1);
            circ.rewire(v1, {in_e}, {EdgeType::Quantum});
            success = true;
            e1 = circ.get_nth_out_edge(v, 1);
            v1 = circ.target(e1);
          }
          if (circ.get_OpType_from_Vertex(v1) == OpType::V) {
            circ.remove_vertex(
                v1, Circuit::GraphRewiring::Yes, Circuit::VertexDeletion::No);
            Edge in_e = circ.get_nth_in_edge(v, 1);
            circ.rewire(v1, {in_e}, {EdgeType::Quantum});
            success = true;
          }
        }
      }
    }
    // clean up the start of the circuit
    for (const Vertex &i : circ.q_inputs()) {
      Edge e = circ.get_nth_out_edge(i, 0);
      success = singleq_clifford_from_edge(circ, e, bin) || success;
    }
    circ.remove_vertices(
        bin, Circuit::GraphRewiring::No, Circuit::VertexDeletion::Yes);
    return success;
  });
}

struct MeasureVertices {
  Qubit qubit;
  Bit bit;
  Vertex measure;
  MeasureVertices(const Qubit &q, const Bit &b, const Vertex &v)
      : qubit(q), bit(b), measure(v) {}
};

std::pair<VertexSet, std::vector<MeasureVertices>> get_end_of_circuit_clifford(
    const Circuit &circ) {
  // Initialize vector to store MeasureVertices for end-of-circuit Measures
  std::vector<MeasureVertices> end_of_circuit_measures;

  // Iterate over Qubit boundaries to find Measure gates
  for (auto [it, end] =
           circ.boundary.get<TagType>().equal_range(UnitType::Qubit);
       it != end; it++) {
    Vertex q_out = it->out_;
    Edge last_gate_out_edge = circ.get_nth_in_edge(q_out, 0);
    Vertex last_gate = circ.source(last_gate_out_edge);

    // Check if last gate is a Measure gate
    if (circ.get_OpType_from_Vertex(last_gate) == OpType::Measure) {
      Edge possible_c_out_in_edge = circ.get_nth_out_edge(last_gate, 1);
      Vertex possible_c_out = circ.target(possible_c_out_in_edge);

      // If the Measure is followed by a ClOutput, store the MeasureVertices
      if (circ.get_OpType_from_Vertex(possible_c_out) == OpType::ClOutput) {
        Bit b(circ.get_id_from_out(possible_c_out));
        end_of_circuit_measures.push_back(
            MeasureVertices(Qubit(it->id_), b, last_gate));
      }
    }
  }

  // Initialize variables for constructing Clifford circuit
  VertexSet clifford_vertices;
  std::map<Edge, Qubit> frontier;
  VertexSet previous;

  // Populate frontier map with Measure edges and associated qubits
  for (const auto &mv : end_of_circuit_measures) {
    frontier.insert({circ.get_nth_in_edge(mv.measure, 0), mv.qubit});
  }

  // Start adding Clifford vertices to set of vertices
  std::vector<std::pair<OpType, std::vector<Qubit>>> clifford_commands;
  std::vector<Edge> to_erase;
  while (!frontier.empty()) {
    VertexSet current;
    // Iterate over frontier edges to identify Clifford gates
    for (const auto &e : frontier) {
      Vertex source = circ.source(e.first);
      OpDesc desc = circ.get_OpDesc_from_Vertex(source);
      // Check if gate is a Clifford gate without classical or boolean inputs
      if (desc.is_clifford_gate() && desc.n_boolean() == 0 &&
          desc.n_classical() == 0) {
        current.insert(source);
      } else {
        // Remove non-Clifford gates from frontier
        to_erase.push_back(e.first);
      }
    }
    for (const Edge &e : to_erase) {
      frontier.erase(e);
    }
    // Check if identical to previous vertices
    if (current == previous) {
      break;
    }
    // Update previous set with current set
    previous = current;

    // Process Clifford gates and update frontier
    for (const Vertex &v : current) {
      EdgeVec out_edges = circ.get_all_out_edges(v);
      std::vector<Edge> edges;
      std::vector<Qubit> qubits;

      // Check which edges connect to existing qubits in the frontier
      for (const Edge &edge : out_edges) {
        auto it = frontier.find(edge);
        if (it != frontier.end()) {
          edges.push_back(it->first);
          qubits.push_back(it->second);
        }
      }

      // If all outgoing edges connect to frontier qubits, add gate to Clifford
      // circuit
      if (qubits.size() == out_edges.size()) {
        clifford_vertices.insert(v);
        EdgeVec in_edges = circ.get_in_edges(v);

        // Update frontier with incoming edges to processed gate
        for (unsigned i = 0; i < edges.size(); i++) {
          Edge e = edges[i];
          port_t source = circ.get_source_port(e);
          frontier.insert({in_edges[source], qubits[i]});
          frontier.erase(e);
        }
      }
    }
  }
  return std::make_pair(clifford_vertices, end_of_circuit_measures);
}

Transform push_cliffords_through_measures() {
  return Transform([](Circuit &circ) {
    // Extract Clifford and Measure vertices information from provided circuit
    std::pair<VertexSet, std::vector<MeasureVertices>> clifford_info =
        get_end_of_circuit_clifford(circ);
    VertexSet clifford_vertices = clifford_info.first;
    std::vector<MeasureVertices> measure_vertices = clifford_info.second;

    std::vector<Qubit> qubits;
    std::vector<Bit> bits;
    std::map<Edge, Qubit> measured_wires;
    VertexSet final_measure_vertices;

    for (const MeasureVertices &mv : measure_vertices) {
      qubits.push_back(mv.qubit);
      bits.push_back(mv.bit);
      measured_wires.emplace(circ.get_nth_in_edge(mv.measure, 0), mv.qubit);
      final_measure_vertices.insert(mv.measure);
    }

    const Subcircuit clifford = circ.make_subcircuit(clifford_vertices);
    const Circuit clifford_circuit = circ.subcircuit(clifford);
    const UnitaryRevTableau cliff_tab =
        circuit_to_unitary_rev_tableau(clifford_circuit);

    // The extracted circuit numbers its wires in boundary order. Each output
    // edge leads to a final measurement in the parent circuit.
    boost::bimap<Qubit, Qubit> local_to_parent;
    for (unsigned i = 0; i < clifford.out_hole.size(); ++i) {
      local_to_parent.insert(
          {Qubit(i), measured_wires.at(clifford.out_hole[i].value())});
    }

    std::list<SpSymPauliTensor> measurement_operators;
    for (const Qubit &q : qubits) {
      auto local = local_to_parent.right.find(q);
      if (local == local_to_parent.right.end()) {
        measurement_operators.push_back(SpPauliStabiliser(q, Pauli::Z, 2));
        continue;
      }
      const SpPauliStabiliser local_op = cliff_tab.get_row_product(
          SpPauliStabiliser(local->second, Pauli::Z, 2));
      QubitPauliMap parent_string;
      for (const auto &[local_qubit, pauli] : local_op.string) {
        parent_string.emplace(local_to_parent.left.at(local_qubit), pauli);
      }
      measurement_operators.push_back(
          SpPauliStabiliser(parent_string, local_op.coeff));
    }

    // Mutually diagonalize updated measurement operators
    Circuit mutual_c = mutual_diagonalise(
        measurement_operators, std::set<Qubit>(qubits.begin(), qubits.end()),
        CXConfigType::Snake);

    // If mutual diagonalisation circuit doesn't improve 2-qubit gate
    // count then keep circuit as is
    if (mutual_c.count_n_qubit_gates(2) >=
        clifford_circuit.count_n_qubit_gates(2)) {
      return false;
    }

    // Remove Clifford and Measure vertices from the original circuit
    // before adding mutual diagonalisation circuit
    circ.remove_vertices(
        clifford_vertices, Circuit::GraphRewiring::Yes,
        Circuit::VertexDeletion::Yes);
    circ.remove_vertices(
        final_measure_vertices, Circuit::GraphRewiring::Yes,
        Circuit::VertexDeletion::Yes);
    // add mutual diagonalisation circuit
    circ.append(mutual_c);

    // add back in end of circuit measurements
    TKET_ASSERT(qubits.size() == bits.size());
    for (unsigned i = 0; i < qubits.size(); i++) {
      circ.add_measure(qubits[i], bits[i]);
    }

    // Add classical logic to permute output measurements to correct result
    std::string reg_name =
        circ.get_next_c_reg_name(c_permutation_scratch_name());
    register_t scratch_r = circ.add_c_register(reg_name, bits.size() + 1);
    // Convert Bit to vector for ease of indexing and assigning
    std::vector<Bit> scratch_v;
    for (const auto &b : scratch_r) {
      Bit scratch_b = Bit(b.second);
      scratch_v.push_back(scratch_b);
    }

    // We need to collect ClassicalX due to phase correction
    // and permutation due to Z terms in operator
    std::vector<Bit> phase_correction;

    TKET_ASSERT(measurement_operators.size() == bits.size());
    auto it = measurement_operators.begin();
    for (unsigned i = 0; i < measurement_operators.size(); ++i, ++it) {
      // For each measurement operator, we collect the qubits in the string
      // which have Z terms
      auto string = it->string;
      std::vector<Bit> parity_bits;
      for (unsigned j = 0; j < qubits.size(); j++) {
        Qubit q = qubits[j];
        auto jt = string.find(q);
        if (jt != string.end()) {
          // If the qubit has a Z term, then it's measurement result
          // needs to be Xored with the original target qubit of the
          // measurement
          if (jt->second == Pauli::Z) {
            parity_bits.push_back(bits[j]);
          } else {
            TKET_ASSERT(jt->second == Pauli::I);
          }
        }
      }
      // The measurement operator should never be empty, meaning
      // that parity_bits should always have at least one entry
      TKET_ASSERT(!parity_bits.empty());
      // we now add the Xor corresponding to the correction
      for (const Bit &pb : parity_bits) {
        std::vector<Bit> arg = {pb, Bit(scratch_v[i])};
        circ.add_op(XorWithOp(), arg);
      }

      // Finally, we check the phase, and store the required Bit
      // to flip as appropriate
      if (it->coeff == 1) {
        phase_correction.push_back(scratch_v[i]);
      } else {
        TKET_ASSERT(it->coeff == -1);
      }
    }

    // Apply the constant change to bitstring due to phase
    circ.add_op(
        std::make_shared<SetBitsOp>(std::vector<bool>({true})),
        std::vector<Bit>({scratch_v.back()}));
    for (const auto &p_bit : phase_correction) {
      circ.add_op(XorWithOp(), std::vector<Bit>({scratch_v.back(), p_bit}));
    }

    // Copy scratch results over to original Bit
    TKET_ASSERT(bits.size() + 1 == scratch_v.size());
    scratch_v.pop_back();
    scratch_v.insert(scratch_v.end(), bits.begin(), bits.end());
    circ.add_op(std::make_shared<CopyBitsOp>(unsigned(bits.size())), scratch_v);
    return true;
  });
}

}  // namespace Transforms

}  // namespace tket
