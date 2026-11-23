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

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

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
 * Follows an edge in the circuit's DAG until it reaches a non-commuting
 * operation. By default, it allows commutation
 * through SWAPs, gates that commute on the Pauli Z basis, and recursively
 * checks inside boxes and conditional operations.
 * @param circ The circuit
 * @param current_edge The edge to start following.
 * @param only_boxes Whether to only commute through straight wires in boxes or
 * conditional operations.
 * @return The edge leading to the first operation that cannot be crossed, or to
 * the quantum output.
 **/
static Edge follow_until_noncommuting(
    const Circuit& circ, Edge current_edge, bool only_boxes);

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
    const Circuit& circ, Edge current_edge, bool only_boxes) {
  port_t current_port = circ.get_target_port(current_edge);
  Vertex current_vertex = circ.target(current_edge);
  OpType current_optype = circ.get_OpType_from_Vertex(current_vertex);
  while (!is_final_q_type(current_optype)) {
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

class CommandOrder {
 public:
  CommandOrder(VertexVec commands, std::size_t work_budget)
      : commands_(std::move(commands)), remaining_work_(work_budget) {
    positions_.reserve(commands_.size());
    for (std::size_t i = 0; i < commands_.size(); ++i) {
      positions_.emplace(commands_[i], i);
    }
  }

  std::optional<std::size_t> position(Vertex v) const {
    const auto it = positions_.find(v);
    if (it == positions_.end()) return std::nullopt;
    return it->second;
  }

  // An incomplete repair leaves this index unusable.
  bool update_after_move(
      const Circuit& circ, Vertex measure, Vertex last_gate) {
    const std::size_t first = positions_.at(measure);
    const std::size_t last = positions_.at(last_gate);
    // Charge for both scanning the interval and reindexing it.
    if (!spend_work(2 * (last - first + 1))) return false;
    VertexSet delayed{measure};
    VertexVec delayed_in_order;
    std::size_t next = first;

    // Only the new edge from last_gate to measure goes backward in the old
    // order. Move the measurement and its descendants in this interval after
    // last_gate, preserving the relative order within each group.
    const auto defer_if_in_interval = [&](Vertex successor) {
      const auto rank = position(successor);
      if (rank && *rank <= last) delayed.insert(successor);
    };
    for (std::size_t i = first; i <= last; ++i) {
      const Vertex v = commands_[i];
      if (!delayed.contains(v)) {
        commands_[next++] = v;
        continue;
      }
      if (!spend_work(std::size_t{circ.n_in_edges(v)} + circ.n_out_edges(v))) {
        return false;
      }
      delayed_in_order.push_back(v);
      for (const Edge& out : circ.get_all_out_edges(v)) {
        defer_if_in_interval(circ.target(out));
      }
      // Each Boolean read precedes the next write to that bit.
      for (const Edge& read : circ.get_in_edges_of_type(v, EdgeType::Boolean)) {
        // Resolving this dependency can scan the value's outgoing edges.
        if (!spend_work(circ.n_out_edges(circ.source(read)))) return false;
        const Vertex next_write = circ.target(circ.get_linear_edge(read));
        if (next_write != v) defer_if_in_interval(next_write);
      }
    }
    for (Vertex v : delayed_in_order) {
      commands_[next++] = v;
    }
    for (std::size_t i = first; i <= last; ++i) {
      positions_.at(commands_[i]) = i;
    }
    return true;
  }

 private:
  bool spend_work(std::size_t work) {
    if (work > remaining_work_) return false;
    remaining_work_ -= work;
    return true;
  }

  VertexVec commands_;
  IndexMap positions_;
  std::size_t remaining_work_;
};

static bool depends_on_measurement_result(
    const Circuit& circ, Vertex gate, const VertexSet& readers,
    const CommandOrder* order, std::size_t first_reader, VertexSet& visited) {
  VertexVec pending{gate};
  while (!pending.empty()) {
    Vertex current = pending.back();
    pending.pop_back();
    if (!visited.insert(current).second) continue;
    if (order) {
      // Boundaries are unranked. Inputs precede every reader and outputs cannot
      // be ancestors of a gate.
      const auto rank = order->position(current);
      if (!rank || *rank < first_reader) continue;
    }
    if (readers.contains(current)) return true;
    for (const Edge& input : circ.get_in_edges(current)) {
      Vertex source = circ.source(input);
      pending.push_back(source);
      if (circ.get_edgetype(input) != EdgeType::Classical) continue;
      // A write must also wait for readers of the previous value on this port.
      for (const Edge& read :
           circ.get_nth_b_out_bundle(source, circ.get_source_port(input))) {
        Vertex reader = circ.target(read);
        if (reader != current) pending.push_back(reader);
      }
    }
  }
  return false;
}

static Edge find_partial_delay_destination(
    const Circuit& circ, Vertex measure, Edge current_edge,
    const CommandOrder* order) {
  std::optional<VertexSet> readers;
  std::size_t first_reader = 0;
  VertexSet visited;
  Vertex gate = circ.target(current_edge);
  while (!circ.detect_final_Op(gate)) {
    const auto next_port = op_commutes(
        circ.get_Op_ptr_from_Vertex(gate), circ.get_target_port(current_edge),
        false);
    if (!next_port) break;

    // Dependencies from result readers can only enter this wire through an
    // additional input.
    if (circ.n_in_edges(gate) > 1) {
      if (!readers) {
        const auto direct_readers =
            circ.get_successors_of_type(measure, EdgeType::Boolean);
        if (direct_readers.empty()) {
          return follow_until_noncommuting(
              circ, circ.get_nth_out_edge(gate, *next_port), false);
        }
        readers.emplace(direct_readers.begin(), direct_readers.end());
        if (order) {
          first_reader = order->position(direct_readers.front()).value();
          for (Vertex reader : direct_readers) {
            first_reader =
                std::min(first_reader, order->position(reader).value());
          }
        }
        visited.insert(measure);
      }
      // The preceding vertex is already safe. All vertices visited by an
      // earlier search are also safe, since we stop at the first reader found.
      visited.insert(circ.source(current_edge));
      if (depends_on_measurement_result(
              circ, gate, *readers, order, first_reader, visited)) {
        break;
      }
    }

    current_edge = circ.get_nth_out_edge(gate, *next_port);
    gate = circ.target(current_edge);
  }
  return current_edge;
}

static bool delay_measures_partially(Circuit& circ) {
  VertexVec commands;
  commands.reserve(circ.n_gates());
  VertexVec measurements;
  bool has_result_readers = false;
  // Command order already respects read-before-overwrite dependencies.
  for (const Command& com : circ) {
    const Vertex v = com.get_vertex();
    commands.push_back(v);
    if (com.get_op_ptr()->get_type() != OpType::Measure) continue;
    const Edge c_out_edge = circ.get_nth_out_edge(v, 1);
    if (!circ.detect_final_Op(circ.target(c_out_edge))) continue;
    measurements.push_back(v);
    has_result_readers = has_result_readers ||
                         circ.n_out_edges_of_type(v, EdgeType::Boolean) != 0;
  }
  if (measurements.empty()) return false;
  std::optional<CommandOrder> order;
  if (has_result_readers) {
    constexpr std::size_t order_repair_budget_multiplier = 4;
    const std::size_t graph_size =
        std::size_t{circ.n_vertices()} + circ.n_edges();
    order.emplace(
        std::move(commands), order_repair_budget_multiplier * graph_size);
  }

  // When a later measurement can use the order, every move must maintain it.
  bool changed = false;
  for (Vertex v : measurements) {
    Edge out_edge = circ.get_nth_out_edge(v, 0);
    Edge destination = has_result_readers
                           ? find_partial_delay_destination(
                                 circ, v, out_edge, order ? &*order : nullptr)
                           : follow_until_noncommuting(circ, out_edge, false);
    if (destination == out_edge) continue;
    const Vertex last_gate = circ.source(destination);
    move_measure(circ, v, destination);
    if (order && !order->update_after_move(circ, v, last_gate)) {
      order.reset();
    }
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
