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

#include "tket/Transformations/MeasurePass.hpp"

#include <optional>

#include "tket/Circuit/DAGDefs.hpp"
#include "tket/OpType/EdgeType.hpp"
#include "tket/OpType/OpTypeFunctions.hpp"
#include "tket/Ops/OpPtr.hpp"
#include "tket/Transformations/Transform.hpp"

namespace tket {

namespace Transforms {

Transform delay_measures(bool allow_partial) {
  return Transform([allow_partial](Circuit& circ) {
    return DelayMeasures::run_delay_measures(circ, allow_partial, false).first;
  });
}

namespace DelayMeasures {

bool check_only_end_measures(const Command& com, unit_set_t& measured_units) {
  OpType optype = com.get_op_ptr()->get_type();
  if (optype == OpType::Conditional) {
    unit_vector_t all_args = com.get_args();
    const Conditional& cond =
        static_cast<const Conditional&>(*com.get_op_ptr());
    unit_vector_t::iterator arg_it = all_args.begin();
    for (unsigned i = 0; i < cond.get_width(); ++i) {
      if (measured_units.find(*arg_it) != measured_units.end()) return false;
      ++arg_it;
    }
    unit_vector_t new_args = {arg_it, all_args.end()};
    Command new_com = {cond.get_op(), new_args};
    return check_only_end_measures(new_com, measured_units);
  } else if (optype == OpType::CircBox || optype == OpType::CustomGate) {
    const Box& box = static_cast<const Box&>(*com.get_op_ptr());
    unit_map_t interface;
    unit_set_t inner_set;
    unsigned q_count = 0;
    unsigned b_count = 0;
    for (const UnitID& u : com.get_args()) {
      UnitID inner_unit = (u.type() == UnitType::Qubit)
                              ? static_cast<UnitID>(Qubit(q_count++))
                              : static_cast<UnitID>(Bit(b_count++));
      interface.insert({inner_unit, u});
      if (measured_units.find(u) != measured_units.end()) {
        inner_set.insert(inner_unit);
      }
    }
    for (const Command& c : *box.to_circuit()) {
      if (!check_only_end_measures(c, inner_set)) return false;
    }
    for (const UnitID& u : inner_set) {
      measured_units.insert(interface.at(u));
    }
    return true;
  } else if (optype == OpType::Measure) {
    std::pair<unit_set_t::iterator, bool> q_inserted =
        measured_units.insert(com.get_args().at(0));
    std::pair<unit_set_t::iterator, bool> c_inserted =
        measured_units.insert(com.get_args().at(1));
    return q_inserted.second && c_inserted.second;
  } else {
    for (const UnitID& a : com.get_args()) {
      if (measured_units.find(a) != measured_units.end()) return false;
    }
    return true;
  }
}

/**
 * Follows an edge in the circuit's DAG until it reaches a non-commuting or
 * blocked operation. By default, it allows commutation
 * through SWAPs, gates that commute on the Pauli Z basis, and recursively
 * checks inside boxes and conditional operations.
 * @param circ The circuit
 * @param current_edge The edge to start following.
 * @param only_boxes Whether to only commute through straight wires in boxes or
 * conditional operations.
 * @param blocked_vertices Operations that the measurement must stay before.
 * @return The edge leading to the first operation that cannot be crossed, or to
 * the quantum output.
 **/
static Edge follow_until_noncommuting(
    const Circuit& circ, Edge current_edge, bool only_boxes,
    const VertexSet* blocked_vertices = nullptr);

/**
 * Checks if the operation can commute. By default, it allows commutation
 * through SWAPs, gates that commute on the Pauli Z basis, and recursively
 * checks inside boxes and conditional operations.
 * @param op The operation.
 * @param in_port The input port of the operation to check.
 * @param only_boxes Whether to only commute through straight wires in boxes or
 * conditional operations.
 *basis.
 * @return The output port of the operation if it commutes, or nullopt.
 **/
static std::optional<port_t> op_commutes(
    Op_ptr op, port_t in_port, bool only_boxes) {
  OpType optype = op->get_type();
  if (optype == OpType::SWAP) {
    if (only_boxes) return std::nullopt;
    return 1 - in_port;
  } else if (optype == OpType::Conditional) {
    const Conditional& cond = static_cast<const Conditional&>(*op);
    if (in_port < cond.get_width()) return std::nullopt;
    std::optional<port_t> inner_port =
        op_commutes(cond.get_op(), in_port - cond.get_width(), only_boxes);
    // We can only commute through the conditional gate if it commutes to the
    // same port for both values of the condition, e.g. we ban conditional swap
    // gates
    if (inner_port && (*inner_port + cond.get_width() == in_port))
      return in_port;
    else
      return std::nullopt;
  } else if (optype == OpType::CircBox || optype == OpType::CustomGate) {
    const Box& box = static_cast<const Box&>(*op);
    const Circuit& circ = *box.to_circuit();
    Vertex inner_vertex = circ.all_inputs().at(in_port);
    Edge inner_edge = circ.get_nth_out_edge(inner_vertex, 0);
    inner_edge = follow_until_noncommuting(circ, inner_edge, only_boxes);
    inner_vertex = circ.target(inner_edge);
    if (!is_final_q_type(circ.get_OpType_from_Vertex(inner_vertex))) {
      // The operation does not commute through the box
      return std::nullopt;
    }
    auto outputs = circ.all_outputs();
    port_t box_out_port = std::distance(
        outputs.begin(),
        std::find(outputs.begin(), outputs.end(), inner_vertex));
    return box_out_port;
  } else {
    if (only_boxes) return std::nullopt;
    if (op->commutes_with_basis(Pauli::Z, in_port)) return in_port;
    return std::nullopt;
  }
}

static Edge follow_until_noncommuting(
    const Circuit& circ, Edge current_edge, bool only_boxes,
    const VertexSet* blocked_vertices) {
  port_t current_port = circ.get_target_port(current_edge);
  Vertex current_vertex = circ.target(current_edge);
  OpType current_optype = circ.get_OpType_from_Vertex(current_vertex);
  while (!is_final_q_type(current_optype)) {
    if (blocked_vertices && blocked_vertices->contains(current_vertex)) break;
    Op_ptr op = circ.get_Op_ptr_from_Vertex(current_vertex);
    std::optional<port_t> next_port = op_commutes(op, current_port, only_boxes);
    if (next_port.has_value()) {
      current_port = next_port.value();
    } else {
      break;
    }
    // Proceed to successor
    current_edge = circ.get_nth_out_edge(current_vertex, current_port);
    current_vertex = circ.target(current_edge);
    current_port = circ.get_target_port(current_edge);
    current_optype = circ.get_OpType_from_Vertex(current_vertex);
  }
  return current_edge;
}

static void move_measure(Circuit& circ, Vertex v, Edge destination) {
  Edge in_edge = circ.get_nth_in_edge(v, 0);
  Edge out_edge = circ.get_nth_out_edge(v, 0);
  circ.add_edge(
      {circ.source(in_edge), circ.get_source_port(in_edge)},
      {circ.target(out_edge), circ.get_target_port(out_edge)},
      EdgeType::Quantum);
  circ.remove_edge(in_edge);
  circ.remove_edge(out_edge);
  circ.add_edge(
      {circ.source(destination), circ.get_source_port(destination)}, {v, 0},
      EdgeType::Quantum);
  circ.add_edge(
      {v, 0}, {circ.target(destination), circ.get_target_port(destination)},
      EdgeType::Quantum);
  circ.remove_edge(destination);
}

static VertexSet measurement_result_dependents(const Circuit& circ, Vertex v) {
  VertexSet dependents;
  VertexVec pending = circ.get_successors_of_type(v, EdgeType::Boolean);
  while (!pending.empty()) {
    Vertex dependent = pending.back();
    pending.pop_back();
    if (!dependents.insert(dependent).second) continue;
    for (Vertex successor : circ.get_successors(dependent)) {
      pending.push_back(successor);
    }
    // The next write to a bit must wait for readers of its previous value.
    for (const Edge& read :
         circ.get_in_edges_of_type(dependent, EdgeType::Boolean)) {
      Vertex next_write = circ.target(circ.get_linear_edge(read));
      if (next_write != dependent) pending.push_back(next_write);
    }
  }
  return dependents;
}

static bool delay_measures_partially(Circuit& circ) {
  VertexVec measurements;
  for (const Command& com : circ) {
    if (com.get_op_ptr()->get_type() == OpType::Measure) {
      measurements.push_back(com.get_vertex());
    }
  }

  // Each move may change which operations depend on a later measurement.
  bool changed = false;
  for (Vertex v : measurements) {
    Edge c_out_edge = circ.get_nth_out_edge(v, 1);
    if (!circ.detect_final_Op(circ.target(c_out_edge))) continue;
    Edge out_edge = circ.get_nth_out_edge(v, 0);
    if (circ.detect_final_Op(circ.target(out_edge))) continue;
    const VertexSet dependents = measurement_result_dependents(circ, v);
    Edge destination =
        follow_until_noncommuting(circ, out_edge, false, &dependents);
    if (destination == out_edge) continue;
    move_measure(circ, v, destination);
    changed = true;
  }
  return changed;
}

std::pair<bool, bool> run_delay_measures(
    Circuit& circ, bool allow_partial, bool dry_run) {
  if (allow_partial) {
    return {!dry_run && delay_measures_partially(circ), true};
  }
  // In strict mode, check the whole circuit before moving any measurement.
  std::vector<std::pair<Vertex, Edge>> to_delay;
  for (const Command& com : circ) {
    Vertex v = com.get_vertex();
    OpType optype = com.get_op_ptr()->get_type();
    if (optype == OpType::Measure) {
      Edge c_out_edge = circ.get_nth_out_edge(v, 1);
      bool measurement_is_final = circ.detect_final_Op(circ.target(c_out_edge));
      if (!measurement_is_final ||
          circ.n_out_edges_of_type(v, EdgeType::Boolean) != 0) {
        if (dry_run) return {false, false};
        throw CircuitInvalidity(
            "Cannot commute Measure through classical operations to the end "
            "of the circuit");
      }
      Edge out_edge = circ.get_nth_out_edge(v, 0);
      Edge current_edge = follow_until_noncommuting(circ, out_edge, false);
      Vertex current_vertex = circ.target(current_edge);
      // If we haven't reached an output, we can't continue
      if (!is_final_q_type(circ.get_OpType_from_Vertex(current_vertex))) {
        if (dry_run) return {false, false};
        throw CircuitInvalidity(
            "Cannot commute Measure through quantum gates to the end of the "
            "circuit");
      }
      if (dry_run) continue;
      // If the measure was already at the output, nothing to do
      if (current_edge == out_edge) continue;
      to_delay.push_back({v, current_edge});
    } else {
      // Raise an error if there are any boxes or conditionals with internal
      // mid-measures.
      unit_set_t measured_units;
      if (!check_only_end_measures(com, measured_units)) {
        if (dry_run) return {false, false};
        throw optype == OpType::Conditional
            ? CircuitInvalidity("Cannot delay measures inside a conditional")
            : CircuitInvalidity("Cannot delay measures inside a circuit box");
      }
      // If the command has internal end measurements we allow them only if they
      // are at the end of the full circuit. We can't delay the measures inside
      // boxes and conditionals.
      for (const auto& unit : measured_units) {
        if (unit.type() == UnitType::Bit) continue;
        auto args = com.get_args();
        port_t out_port = std::distance(
            args.begin(), std::find(args.begin(), args.end(), unit));
        Edge current_edge = circ.get_nth_out_edge(v, out_port);
        // Follow the edge through boxes
        current_edge = follow_until_noncommuting(circ, current_edge, true);
        Vertex current_vertex = circ.target(current_edge);
        optype = circ.get_OpType_from_Vertex(current_vertex);
        if (!is_final_q_type(optype)) {
          if (dry_run) return {false, false};
          throw optype == OpType::Conditional
              ? CircuitInvalidity("Cannot delay measures inside a conditional")
              : CircuitInvalidity("Cannot delay measures inside a circuit box");
        }
      }
    }
  }

  for (const auto& [v, destination] : to_delay) {
    move_measure(circ, v, destination);
  }
  return {!to_delay.empty(), true};
}

}  // namespace DelayMeasures

}  // namespace Transforms

}  // namespace tket
