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

#include "tket/PauliGraph/PauliGraph.hpp"

#include <tkassert/Assert.hpp>

#include "tket/Gate/Gate.hpp"
#include "tket/OpType/OpType.hpp"
#include "tket/Utils/PauliTensor.hpp"
#include "tket/Utils/UnitID.hpp"

namespace tket {

bool operator<(
    const PauliGadgetProperties &pgp1, const PauliGadgetProperties &pgp2) {
  return (SpPauliString)pgp1.tensor_ < (SpPauliString)pgp2.tensor_;
}

PauliGraph::PauliGraph(unsigned n) : cliff_(n) {}

PauliGraph::PauliGraph(const qubit_vector_t &qbs, const bit_vector_t &bits)
    : cliff_(qbs), bits_(bits) {}

PauliVertSet PauliGraph::get_successors(const PauliVert &vert) const {
  PauliVertSet succs;
  for (auto iter = boost::adjacent_vertices(vert, graph_);
       iter.first != iter.second; iter.first++) {
    succs.insert(*iter.first);
  }
  return succs;
}

PauliVertSet PauliGraph::get_predecessors(const PauliVert &vert) const {
  PauliVertSet preds;
  for (auto iter = boost::inv_adjacent_vertices(vert, graph_);
       iter.first != iter.second; iter.first++) {
    preds.insert(*iter.first);
  }
  return preds;
}

PauliEdgeSet PauliGraph::get_in_edges(const PauliVert &vert) const {
  PauliEdgeSet ins;
  for (auto iter = boost::in_edges(vert, graph_); iter.first != iter.second;
       iter.first++) {
    ins.insert(*iter.first);
  }
  return ins;
}

PauliEdgeSet PauliGraph::get_out_edges(const PauliVert &vert) const {
  PauliEdgeSet outs;
  for (auto iter = boost::out_edges(vert, graph_); iter.first != iter.second;
       iter.first++) {
    outs.insert(*iter.first);
  }
  return outs;
}

PauliVert PauliGraph::source(const PauliEdge &edge) const {
  return boost::source(edge, graph_);
}

PauliVert PauliGraph::target(const PauliEdge &edge) const {
  return boost::target(edge, graph_);
}

void PauliGraph::apply_gate_at_end(
    const Gate &gate, const unit_vector_t &args) {
  for (const UnitID &arg : args) {
    if (arg.type() == UnitType::Qubit) {
      if (measures_.left.find(Qubit(arg)) != measures_.left.end()) {
        throw MidCircuitMeasurementNotAllowed(
            "PauliGraph does not support mid-circuit measurements "
            "- cannot add gate after measure on qubit " +
            arg.repr());
      }
    } else if (measures_.right.find(Bit(arg)) != measures_.right.end()) {
      throw MidCircuitMeasurementNotAllowed(
          "PauliGraph does not support mid-circuit measurements - "
          "cannot add gate after measure to bit " +
          arg.repr());
    }
  }
  OpType type = gate.get_type();
  if (type == OpType::Measure) {
    measures_.insert({Qubit(args.at(0)), Bit(args.at(1))});
    return;
  }
  qubit_vector_t qbs = {args.begin(), args.end()};
  switch (type) {
    case OpType::Z:
    case OpType::X:
    case OpType::Y:
    case OpType::S:
    case OpType::Sdg:
    case OpType::V:
    case OpType::Vdg:
    case OpType::H:
    case OpType::SX:
    case OpType::SXdg:
    case OpType::CX:
    case OpType::CY:
    case OpType::CZ:
    case OpType::SWAP:
    case OpType::noop:
    case OpType::Phase: {
      cliff_.apply_gate_at_end(type, qbs);
      break;
    }
    case OpType::Rz: {
      SpPauliStabiliser pauli = cliff_.get_zrow(qbs.at(0));
      Expr angle = gate.get_params().at(0);
      std::optional<unsigned> cliff_angle = equiv_Clifford(angle);
      if (cliff_angle) {
        for (unsigned i = 0; i < cliff_angle.value(); i++) {
          cliff_.apply_gate_at_end(OpType::S, qbs);
        }
      } else
        apply_pauli_gadget_at_end(pauli, angle);
      break;
    }
    case OpType::Rx: {
      SpPauliStabiliser pauli = cliff_.get_xrow(qbs.at(0));
      Expr angle = gate.get_params().at(0);
      std::optional<unsigned> cliff_angle = equiv_Clifford(angle);
      if (cliff_angle) {
        for (unsigned i = 0; i < cliff_angle.value(); i++) {
          cliff_.apply_gate_at_end(OpType::V, qbs);
        }
      } else
        apply_pauli_gadget_at_end(pauli, angle);
      break;
    }
    case OpType::Ry: {
      Expr angle = gate.get_params().at(0);
      std::optional<unsigned> cliff_angle = equiv_Clifford(angle);
      if (cliff_angle) {
        if (cliff_angle.value() != 0) {
          cliff_.apply_gate_at_end(OpType::V, qbs);
          for (unsigned i = 0; i < cliff_angle.value(); i++) {
            cliff_.apply_gate_at_end(OpType::S, qbs);
          }
          cliff_.apply_gate_at_end(OpType::Vdg, qbs);
        }
      } else {
        SpPauliStabiliser ypauli =
            cliff_.get_row_product(SpPauliStabiliser(qbs.at(0), Pauli::Y));
        apply_pauli_gadget_at_end(ypauli, angle);
      }
      break;
    }
    case OpType::PhasedX: {
      Expr alpha = gate.get_params().at(0);
      Expr beta = gate.get_params().at(1);
      std::optional<unsigned> cliff_alpha = equiv_Clifford(alpha);
      std::optional<unsigned> cliff_beta = equiv_Clifford(beta);
      // Rz(-b)
      if (cliff_beta) {
        for (unsigned i = 0; i < cliff_beta.value(); i++) {
          cliff_.apply_gate_at_end(OpType::Sdg, qbs);
        }
      } else {
        SpPauliStabiliser zpauli = cliff_.get_zrow(qbs.at(0));
        apply_pauli_gadget_at_end(zpauli, -beta);
      }
      // Rx(a)
      if (cliff_alpha) {
        for (unsigned i = 0; i < cliff_alpha.value(); i++) {
          cliff_.apply_gate_at_end(OpType::V, qbs);
        }
      } else {
        SpPauliStabiliser xpauli = cliff_.get_xrow(qbs.at(0));
        apply_pauli_gadget_at_end(xpauli, alpha);
      }
      // Rz(b)
      if (cliff_beta) {
        for (unsigned i = 0; i < cliff_beta.value(); i++) {
          cliff_.apply_gate_at_end(OpType::S, qbs);
        }
      } else {
        SpPauliStabiliser zpauli = cliff_.get_zrow(qbs.at(0));
        apply_pauli_gadget_at_end(zpauli, beta);
      }
      break;
    }
    case OpType::T: {
      SpPauliStabiliser pauli = cliff_.get_zrow(qbs.at(0));
      apply_pauli_gadget_at_end(pauli, 0.25);
      break;
    }
    case OpType::Tdg: {
      SpPauliStabiliser pauli = cliff_.get_zrow(qbs.at(0));
      apply_pauli_gadget_at_end(pauli, -0.25);
      break;
    }
    case OpType::ZZMax: {
      cliff_.apply_gate_at_end(OpType::S, {qbs.at(0)});
      cliff_.apply_gate_at_end(OpType::Z, {qbs.at(1)});
      cliff_.apply_gate_at_end(OpType::S, {qbs.at(1)});
      cliff_.apply_gate_at_end(OpType::V, {qbs.at(1)});
      cliff_.apply_gate_at_end(OpType::S, {qbs.at(1)});
      cliff_.apply_gate_at_end(OpType::CX, qbs);
      cliff_.apply_gate_at_end(OpType::S, {qbs.at(1)});
      cliff_.apply_gate_at_end(OpType::V, {qbs.at(1)});
      break;
    }
    case OpType::PhaseGadget:
    case OpType::ZZPhase: {
      Expr angle = gate.get_params().at(0);
      std::optional<unsigned> cliff_angle = equiv_Clifford(angle);
      if (cliff_angle) {
        if (cliff_angle.value() != 0) {
          QubitPauliMap qpm;
          for (const Qubit &q : qbs) qpm.insert({q, Pauli::Z});
          cliff_.apply_pauli_at_end(SpPauliStabiliser(qpm), *cliff_angle);
        }
      } else {
        QubitPauliMap qpm;
        for (const Qubit &q : qbs) qpm.insert({q, Pauli::Z});
        SpPauliStabiliser pauli =
            cliff_.get_row_product(SpPauliStabiliser(qpm));
        apply_pauli_gadget_at_end(pauli, angle);
      }
      break;
    }
    case OpType::XXPhase: {
      Expr angle = gate.get_params().at(0);
      std::optional<unsigned> cliff_angle = equiv_Clifford(angle);
      if (cliff_angle) {
        if (cliff_angle.value() != 0) {
          cliff_.apply_pauli_at_end(
              SpPauliStabiliser({{qbs.at(0), Pauli::X}, {qbs.at(1), Pauli::X}}),
              *cliff_angle);
        }
      } else {
        SpPauliStabiliser pauli = cliff_.get_row_product(
            SpPauliStabiliser({{qbs.at(0), Pauli::X}, {qbs.at(1), Pauli::X}}));
        apply_pauli_gadget_at_end(pauli, angle);
      }
      break;
    }
    case OpType::YYPhase: {
      Expr angle = gate.get_params().at(0);
      std::optional<unsigned> cliff_angle = equiv_Clifford(angle);
      if (cliff_angle) {
        if (cliff_angle.value() != 0) {
          cliff_.apply_pauli_at_end(
              SpPauliStabiliser({{qbs.at(0), Pauli::Y}, {qbs.at(1), Pauli::Y}}),
              *cliff_angle);
        }
      } else {
        SpPauliStabiliser pauli = cliff_.get_row_product(
            SpPauliStabiliser({{qbs.at(0), Pauli::Y}, {qbs.at(1), Pauli::Y}}));
        apply_pauli_gadget_at_end(pauli, angle);
      }
      break;
    }
    case OpType::TwinPhasedX: {
      const Expr alpha = gate.get_params().at(0);
      const Expr beta = gate.get_params().at(1);
      std::optional<unsigned> cliff_alpha = equiv_Clifford(alpha);
      std::optional<unsigned> cliff_beta = equiv_Clifford(beta);
      for (unsigned i = 0; i < 2; i++) {  // PhasedX on qubit i
        const Qubit qb = qbs.at(i);
        // Rz(-b)
        if (cliff_beta) {
          for (unsigned i = 0; i < cliff_beta.value(); i++) {
            cliff_.apply_gate_at_end(OpType::Sdg, {qb});
          }
        } else {
          SpPauliStabiliser zpauli = cliff_.get_zrow(qb);
          apply_pauli_gadget_at_end(zpauli, -beta);
        }
        // Rx(a)
        if (cliff_alpha) {
          for (unsigned i = 0; i < cliff_alpha.value(); i++) {
            cliff_.apply_gate_at_end(OpType::V, {qb});
          }
        } else {
          SpPauliStabiliser xpauli = cliff_.get_xrow(qb);
          apply_pauli_gadget_at_end(xpauli, alpha);
        }
        // Rz(b)
        if (cliff_beta) {
          for (unsigned i = 0; i < cliff_beta.value(); i++) {
            cliff_.apply_gate_at_end(OpType::S, {qb});
          }
        } else {
          SpPauliStabiliser zpauli = cliff_.get_zrow(qb);
          apply_pauli_gadget_at_end(zpauli, beta);
        }
      }
      break;
    }
    case OpType::PhasedXX: {
      Expr alpha = gate.get_params().at(0);
      Expr beta = gate.get_params().at(1);
      std::optional<unsigned> cliff_alpha = equiv_Clifford(alpha);
      std::optional<unsigned> cliff_beta = equiv_Clifford(beta);
      const Qubit qb0 = qbs.at(0);
      const Qubit qb1 = qbs.at(1);
      // Rz(-b) on qb0 and qb1
      for (unsigned i = 0; i < 2; i++) {
        if (cliff_beta) {
          for (unsigned j = 0; j < cliff_beta.value(); j++) {
            cliff_.apply_gate_at_end(OpType::Sdg, {qbs.at(i)});
          }
        } else {
          apply_pauli_gadget_at_end(cliff_.get_zrow(qbs.at(i)), -beta);
        }
      }
      // XXPhase
      if (cliff_alpha) {
        if (cliff_alpha.value() != 0) {
          cliff_.apply_pauli_at_end(
              SpPauliStabiliser({{qb0, Pauli::X}, {qb1, Pauli::X}}),
              *cliff_alpha);
        }
      } else {
        SpPauliStabiliser pauli = cliff_.get_row_product(
            SpPauliStabiliser({{qb0, Pauli::X}, {qb1, Pauli::X}}));
        apply_pauli_gadget_at_end(pauli, alpha);
      }
      // Rz(b) on qb0 and qb1
      for (unsigned i = 0; i < 2; i++) {
        if (cliff_beta) {
          for (unsigned j = 0; j < cliff_beta.value(); j++) {
            cliff_.apply_gate_at_end(OpType::S, {qbs.at(i)});
          }
        } else {
          apply_pauli_gadget_at_end(cliff_.get_zrow(qbs.at(i)), beta);
        }
      }
      break;
    }
    default: {
      throw BadOpType("Cannot add gate to PauliGraph", type);
    }
  }
}

void PauliGraph::apply_pauli_gadget_at_end(
    const SpPauliStabiliser &pauli, const Expr &angle) {
  PauliVertSet to_search = end_line_;
  PauliVertSet commuted;
  PauliVert new_vert = boost::add_vertex(graph_);
  graph_[new_vert] = {pauli, angle};
  while (!to_search.empty()) {
    // Get next candidate parent
    PauliVert to_compare = *to_search.begin();
    to_search.erase(to_search.begin());

    // Check that we have already commuted past all of its children
    bool ready = true;
    for (const PauliVert &child : get_successors(to_compare)) {
      if (commuted.get<TagKey>().find(child) == commuted.get<TagKey>().end()) {
        ready = false;
        break;
      }
    }
    if (!ready) continue;

    // Check if we can commute past it
    SpPauliStabiliser compare_pauli = graph_[to_compare].tensor_;
    if (pauli.commutes_with(compare_pauli)) {
      if (pauli.string == compare_pauli.string) {
        // Identical strings - we can merge vertices
        if (pauli.is_real_negative() == compare_pauli.is_real_negative()) {
          graph_[to_compare].angle_ += angle;
        } else {
          graph_[to_compare].angle_ -= angle;
        }
        boost::clear_vertex(new_vert, graph_);
        boost::remove_vertex(new_vert, graph_);

        std::optional<unsigned> cl_ang =
            equiv_Clifford(graph_[to_compare].angle_);
        if (cl_ang) {
          cliff_.apply_pauli_at_front(graph_[to_compare].tensor_, *cl_ang);
          start_line_.erase(to_compare);
          for (const PauliVert &v : get_predecessors(to_compare)) {
            if (boost::out_degree(v, graph_) == 1) {
              end_line_.insert(v);
            }
          }
          end_line_.erase(to_compare);
          boost::clear_vertex(to_compare, graph_);
          boost::remove_vertex(to_compare, graph_);
        }
        return;
      } else {
        // Commute and continue searching
        PauliVertSet preds = get_predecessors(to_compare);
        to_search.insert(preds.begin(), preds.end());
        commuted.insert(to_compare);
      }
    } else {
      // Does not commute - add dependency edge
      boost::add_edge(to_compare, new_vert, graph_);
      end_line_.erase(to_compare);
    }
  }
  end_line_.insert(new_vert);
  if (get_predecessors(new_vert).empty()) start_line_.insert(new_vert);
}

void PauliGraph::to_graphviz_file(const std::string &filename) const {
  std::ofstream dot_file(filename);
  to_graphviz(dot_file);
}

void PauliGraph::to_graphviz(std::ostream &out) const {
  out << "digraph G {\n";

  std::map<PauliVert, unsigned> index_map;
  unsigned i = 0;
  BGL_FORALL_VERTICES(v, graph_, PauliDAG) {
    index_map.insert({v, i});
    out << i << " [label = \"" << graph_[v].tensor_.to_str() << ", "
        << graph_[v].angle_ << "\"];\n";
    ++i;
  }

  BGL_FORALL_EDGES(e, graph_, PauliDAG) {
    PauliVert v_so = source(e);
    PauliVert v_ta = target(e);
    out << index_map.at(v_so) << " -> " << index_map.at(v_ta) << ";\n";
  }

  out << "}";
}

std::vector<PauliVert> PauliGraph::vertices_in_order() const {
  PauliVIndex index = boost::get(boost::vertex_index, graph_);
  int i = 0;
  BGL_FORALL_VERTICES(v, graph_, PauliDAG) { boost::put(index, v, i++); }
  std::vector<PauliVert> vertices;
  boost::topological_sort(graph_, std::back_inserter(vertices));
  std::reverse(vertices.begin(), vertices.end());
  return vertices;
}

void PauliGraph::sanity_check() const {
  for (const PauliVert &vert : vertices_in_order()) {
    PauliVertSet succs;
    boost::graph_traits<PauliDAG>::adjacency_iterator ai, a_end;
    boost::tie(ai, a_end) = boost::adjacent_vertices(vert, graph_);
    for (; ai != a_end; ai++) {
      TKET_ASSERT(!succs.contains(*ai));
      succs.insert(*ai);
    }

    PauliVertSet preds;
    PauliDAG::inv_adjacency_iterator iai, ia_end;
    boost::tie(iai, ia_end) = boost::inv_adjacent_vertices(vert, graph_);
    for (; iai != ia_end; iai++) {
      TKET_ASSERT(!preds.contains(*iai));
      preds.insert(*iai);
    }
  }
}

}  // namespace tket
