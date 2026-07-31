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

/////////////////////////////////////////////////////
// ALL METHODS TO PERFORM COMPLEX CIRCUIT MANIPULATION//
/////////////////////////////////////////////////////

#include <memory>
#include <tket/OpType/OpType.hpp>
#include <tklog/TketLog.hpp>

#include "tket/Circuit/CircUtils.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Gate/Gate.hpp"
#include "tket/Gate/OpPtrFunctions.hpp"
#include "tket/Ops/ClassicalOps.hpp"
#include "tket/Ops/OpPtr.hpp"
#include "tket/Utils/Expression.hpp"
#include "tket/Utils/UnitID.hpp"

namespace tket {

vertex_map_t Circuit::copy_graph(
    const Circuit& c2, BoundaryMerge boundary_merge,
    OpGroupTransfer opgroup_transfer) {
  switch (opgroup_transfer) {
    case OpGroupTransfer::Preserve:
      // Fail if any collisions.
      for (const auto& opgroupsig : c2.opgroupsigs) {
        if (opgroupsigs.find(opgroupsig.first) != opgroupsigs.end()) {
          throw CircuitInvalidity("Name collision in inserted circuit");
        }
      }
      // Add inserted opgroups to circuit.
      opgroupsigs.insert(c2.opgroupsigs.begin(), c2.opgroupsigs.end());
      break;
    case OpGroupTransfer::Disallow:
      // Fail if any opgroups.
      if (!c2.opgroupsigs.empty()) {
        throw CircuitInvalidity("Named op groups in inserted circuit");
      }
      break;
    case OpGroupTransfer::Merge:
      // Fail if any mismatched signatures
      for (const auto& opgroupsig : c2.opgroupsigs) {
        if (opgroupsigs.find(opgroupsig.first) != opgroupsigs.end()) {
          if (opgroupsigs[opgroupsig.first] != opgroupsig.second) {
            throw CircuitInvalidity(
                "Name signature mismatch in inserted circuit");
          }
        }
      }
      // Add inserted opgroups to circuit.
      opgroupsigs.insert(c2.opgroupsigs.begin(), c2.opgroupsigs.end());
      break;
    default:
      TKET_ASSERT(opgroup_transfer == OpGroupTransfer::Remove);
      // Ignore inserted opgroups
      break;
  }

  vertex_map_t isomap;
  if (&c2 == this) {
    throw Unsupported(
        "Circuit Cannot currently copy itself using this method. Use * "
        "instead\n");
  }
  BGL_FORALL_VERTICES(v, c2.dag, DAG) {
    Vertex v0 = boost::add_vertex(this->dag);
    this->dag[v0].op = c2.get_Op_ptr_from_Vertex(v);
    if (opgroup_transfer == OpGroupTransfer::Preserve ||
        opgroup_transfer == OpGroupTransfer::Merge) {
      this->dag[v0].opgroup = c2.get_opgroup_from_Vertex(v);
    }
    isomap.insert({v, v0});
  }
  BGL_FORALL_VERTICES(v, c2.dag, DAG) {
    EdgeVec edges = c2.get_in_edges(v);
    Vertex target_v = isomap.find(v)->second;
    for (EdgeVec::iterator e1 = edges.begin(); e1 != edges.end(); ++e1) {
      Vertex old_source_v = c2.source(*e1);
      Vertex source_v = isomap.find(old_source_v)->second;
      add_edge(
          {source_v, get_source_port(*e1)}, {target_v, get_target_port(*e1)},
          c2.dag[*e1].type);
    }
  }

  if (boundary_merge == BoundaryMerge::Yes) {
    for (const BoundaryElement& el : c2.boundary.get<TagID>()) {
      std::string reg_name = el.id_.reg_name();
      register_info_t reg_type = el.reg_info();
      opt_reg_info_t reg_found = get_reg_info(reg_name);
      if (reg_found) {
        if (reg_found.value() != reg_type) {
          throw Unsupported(
              "Cannot merge circuits with different types for "
              "register with name: " +
              reg_name);
        }
        boundary_t::iterator unit_found = boundary.get<TagID>().find(el.id_);
        if (unit_found != boundary.get<TagID>().end()) {
          throw Unsupported(
              "Cannot merge circuits as both contain unit: " + el.id_.repr());
        }
      }
      Vertex new_in = isomap[el.in_];
      Vertex new_out = isomap[el.out_];
      boundary.insert({el.id_, new_in, new_out});
    }
  }
  return isomap;
}

// given two circuits, adds second circuit to first circuit object in parallel
Circuit operator*(const Circuit& c1, const Circuit& c2) {
  // preliminary method to add circuit objects together
  Circuit new_circ;
  new_circ.copy_graph(c1);
  new_circ.copy_graph(c2);
  new_circ.add_phase(c1.get_phase() + c2.get_phase());
  return new_circ;
}

void Circuit::append(const Circuit& c2) { append_with_map(c2, {}); }

// qm is from the units on the second (appended) circuit to the units on the
// first (this) circuit
void Circuit::append_with_map(const Circuit& c2, const unit_map_t& qm) {
  Circuit copy = c2;
  copy.rename_units(qm);

  if ((_number_of_wasm_wires > 0) && (copy._number_of_wasm_wires > 0)) {
    if (copy.get_wasm_file_uid() != get_wasm_file_uid()) {
      throw Unsupported(
          "Cannot append circuits with different wasm uids: " +
          get_wasm_file_uid().emplace("(none)") + " and " +
          copy.get_wasm_file_uid().emplace("(none)"));
    }
  }

  copy.add_wasm_register(_number_of_wasm_wires);
  add_wasm_register(copy._number_of_wasm_wires);
  copy.add_rng_register(_number_of_rng_wires);
  add_rng_register(copy._number_of_rng_wires);

  // Check what we need to do at the joins:
  //   Output  --- Input    ==>   -------------
  //   Output  --- Create   ==>   --- Reset ---
  //   Discard --- Input    ==>   [not allowed]
  //   Discard --- Create   ==>   --- Reset ---
  qubit_vector_t qbs = copy.all_qubits();
  std::set<Qubit> qbs_set(qbs.begin(), qbs.end());
  std::set<Qubit> reset_qbs;
  for (const auto& qb : all_qubits()) {
    if (qbs_set.find(qb) != qbs_set.end()) {
      if (copy.is_created(qb)) {
        reset_qbs.insert(qb);
      } else if (is_discarded(qb)) {
        throw CircuitInvalidity("Cannot append input qubit to discarded qubit");
      }
    }
  }

  // Copy c2 into c1 but do not merge boundaries
  vertex_map_t vm = copy_graph(copy, BoundaryMerge::No);
  const Op_ptr noop = get_op_ptr(OpType::noop);

  // Connect each matching qubit and bit, merging remainder
  for (const BoundaryElement& el : copy.boundary.get<TagID>()) {
    std::string reg_name = el.reg_name();
    register_info_t reg_type = el.reg_info();
    opt_reg_info_t reg_found = get_reg_info(reg_name);
    if (reg_found) {
      if (reg_found.value() != reg_type) {
        throw Unsupported(
            "Cannot append circuits with different types for "
            "register with name: " +
            reg_name);
      }
      boundary_t::iterator unit_found = boundary.get<TagID>().find(el.id_);
      if (unit_found != boundary.get<TagID>().end()) {
        Vertex out = unit_found->out_;
        Vertex in = vm[el.in_];
        // Update map
        BoundaryElement new_elem = *unit_found;
        new_elem.out_ = vm[el.out_];
        boundary.replace(unit_found, new_elem);
        // Tie together
        if (reg_type.first == UnitType::Qubit)
          add_edge({out, 0}, {in, 0}, EdgeType::Quantum);
        else
          add_edge({out, 0}, {in, 0}, EdgeType::Classical);
        dag[out].op = noop;
        dag[in].op = noop;
        remove_vertex(out, GraphRewiring::Yes, VertexDeletion::Yes);
        if (el.type() != UnitType::Qubit ||
            reset_qbs.find(Qubit(el.id_)) == reset_qbs.end()) {
          remove_vertex(in, GraphRewiring::Yes, VertexDeletion::Yes);
        } else {
          dag[in].op = std::make_shared<const Gate>(OpType::Reset);
        }
      } else {
        Vertex new_in = vm[el.in_];
        Vertex new_out = vm[el.out_];
        boundary.insert({el.id_, new_in, new_out});
      }
    } else {
      Vertex new_in = vm[el.in_];
      Vertex new_out = vm[el.out_];
      boundary.insert({el.id_, new_in, new_out});
    }
  }
  add_phase(c2.get_phase());
}

void Circuit::append_qubits(
    const Circuit& c2, const std::vector<unsigned>& qubits,
    const std::vector<unsigned>& bits) {
  unit_map_t qm;
  for (unsigned i = 0; i < qubits.size(); i++) {
    qm.insert({Qubit(i), Qubit(qubits[i])});
  }
  for (unsigned i = 0; i < bits.size(); i++) {
    qm.insert({Bit(i), Bit(bits[i])});
  }
  append_with_map(c2, qm);
}

// given two circuits, adds second circuit to first sequentially by tying qubits
// together and returns a copy of this (to prevent destruction of initial
// circuits)
Circuit operator>>(const Circuit& ci1, const Circuit& ci2) {
  Circuit new_circ = ci1;
  new_circ.append(ci2);
  return new_circ;
}

// Important substitute method. Requires knowledge of the boundary to insert
// into, and the vertices inside which are to be removed when substitution is
// performed. Gives the option to isolate the removed vertices but not delete
// them.
void Circuit::substitute(
    const Circuit& to_insert, const Subcircuit& to_replace,
    VertexDeletion vertex_deletion, OpGroupTransfer opgroup_transfer) {
  if (!to_insert.is_simple()) throw SimpleOnly();
  if (to_insert.n_qubits() + to_insert.n_bits() +
          to_insert._number_of_wasm_wires + to_insert._number_of_rng_wires !=
      to_replace.in_hole.size())
    throw CircuitInvalidity("Subcircuit boundary mismatch to hole");

  vertex_map_t vm = copy_graph(to_insert, BoundaryMerge::No, opgroup_transfer);
  VertexList bin;
  EdgeSet ebin;  // Needs to be a set since subcircuit to replace could be
                 // trivial, essentially rewiring on a cut
  std::map<Edge, Vertex> c_out_map;

  std::set<Qubit> reset_qbs;
  for (const auto& qb : to_insert.all_qubits()) {
    if (to_insert.is_created(qb)) {
      reset_qbs.insert(qb);
    } else if (to_insert.is_discarded(qb)) {
      throw CircuitInvalidity("Cannot substitute discarded qubit");
    }
  }

  const Op_ptr noop = get_op_ptr(OpType::noop);
  const Op_ptr reset = get_op_ptr(OpType::Reset);
  unsigned qubit_id = 0;
  unsigned bit_id = 0;
  unsigned wasm_id = 0;
  unsigned rng_id = 0;
  for (unsigned i = 0; i < to_replace.in_hole.size(); ++i) {
    Edge in_edge = to_replace.in_hole.at(i);
    std::optional<Edge> out_edge = to_replace.out_hole.at(i);
    Vertex in_pred = source(in_edge);
    port_t in_port = get_source_port(in_edge);
    ebin.insert(in_edge);
    switch (get_edgetype(in_edge)) {
      case EdgeType::Quantum: {
        TKET_ASSERT(out_edge.has_value());
        TKET_ASSERT(get_edgetype(*out_edge) == EdgeType::Quantum);
        Vertex inp = vm[to_insert.get_in(Qubit(qubit_id))];
        add_edge({in_pred, in_port}, {inp, 0}, EdgeType::Quantum);
        if (reset_qbs.contains(Qubit(qubit_id))) {
          set_vertex_Op_ptr(inp, reset);
        } else {
          set_vertex_Op_ptr(inp, noop);
          bin.push_back(inp);
        }
        Vertex out_succ = target(*out_edge);
        port_t out_port = get_target_port(*out_edge);
        ebin.insert(*out_edge);
        Vertex outp = vm[to_insert.get_out(Qubit(qubit_id))];
        add_edge({outp, 0}, {out_succ, out_port}, EdgeType::Quantum);
        set_vertex_Op_ptr(outp, noop);
        bin.push_back(outp);
        ++qubit_id;
        break;
      }
      case EdgeType::Classical: {
        TKET_ASSERT(out_edge.has_value());
        TKET_ASSERT(get_edgetype(*out_edge) == EdgeType::Classical);
        Vertex inp = vm[to_insert.get_in(Bit(bit_id))];
        add_edge({in_pred, in_port}, {inp, 0}, EdgeType::Classical);
        set_vertex_Op_ptr(inp, noop);
        bin.push_back(inp);
        Vertex out_succ = target(*out_edge);
        port_t out_port = get_target_port(*out_edge);
        ebin.insert(*out_edge);
        Vertex outp = vm[to_insert.get_out(Bit(bit_id))];
        add_edge({outp, 0}, {out_succ, out_port}, EdgeType::Classical);
        set_vertex_Op_ptr(outp, noop);
        bin.push_back(outp);
        c_out_map.insert({*out_edge, outp});
        ++bit_id;
        break;
      }
      case EdgeType::Boolean: {
        TKET_ASSERT(!out_edge.has_value());
        Vertex inp = vm[to_insert.get_in(Bit(bit_id))];
        Vertex outp = vm[to_insert.get_out(Bit(bit_id))];
        if (to_insert.get_successors_of_type(inp, EdgeType::Classical).at(0) !=
            outp)
          throw CircuitInvalidity(
              "Subcircuit replacement writes to a Bit from a read-only input "
              "to the hole");
        for (const Edge& new_edge :
             get_out_edges_of_type(inp, EdgeType::Boolean)) {
          add_edge(
              {in_pred, in_port}, {target(new_edge), get_target_port(new_edge)},
              EdgeType::Boolean);
        }
        set_vertex_Op_ptr(inp, noop);
        set_vertex_Op_ptr(outp, noop);
        bin.push_back(inp);
        bin.push_back(outp);
        ++bit_id;
        break;
      }
      case EdgeType::WASM: {
        TKET_ASSERT(out_edge.has_value());
        TKET_ASSERT(get_edgetype(*out_edge) == EdgeType::WASM);
        Vertex inp = vm[to_insert.get_in(WasmState(wasm_id))];
        add_edge({in_pred, in_port}, {inp, 0}, EdgeType::WASM);
        set_vertex_Op_ptr(inp, noop);
        bin.push_back(inp);
        Vertex out_succ = target(*out_edge);
        port_t out_port = get_target_port(*out_edge);
        ebin.insert(*out_edge);
        Vertex outp = vm[to_insert.get_out(WasmState(wasm_id))];
        add_edge({outp, 0}, {out_succ, out_port}, EdgeType::WASM);
        set_vertex_Op_ptr(outp, noop);
        bin.push_back(outp);
        ++wasm_id;
        break;
      }
      case EdgeType::RNG: {
        TKET_ASSERT(out_edge.has_value());
        TKET_ASSERT(get_edgetype(*out_edge) == EdgeType::RNG);
        Vertex inp = vm[to_insert.get_in(RngState(rng_id))];
        add_edge({in_pred, in_port}, {inp, 0}, EdgeType::RNG);
        set_vertex_Op_ptr(inp, noop);
        bin.push_back(inp);
        Vertex out_succ = target(*out_edge);
        port_t out_port = get_target_port(*out_edge);
        ebin.insert(*out_edge);
        Vertex outp = vm[to_insert.get_out(RngState(rng_id))];
        add_edge({outp, 0}, {out_succ, out_port}, EdgeType::RNG);
        set_vertex_Op_ptr(outp, noop);
        bin.push_back(outp);
        ++rng_id;
        break;
      }
    }
  }
  for (const Edge& e : to_replace.b_future) {
    Edge c_out = get_nth_out_edge(source(e), get_source_port(e));
    Vertex outp = c_out_map.at(c_out);
    add_edge({outp, 0}, {target(e), get_target_port(e)}, EdgeType::Boolean);
    ebin.insert(e);
  }
  for (const Edge& e : ebin) {
    remove_edge(e);
  }
  // automatically rewire these canned vertices
  remove_vertices(bin, GraphRewiring::Yes, VertexDeletion::Yes);
  remove_vertices(to_replace.verts, GraphRewiring::No, vertex_deletion);
  add_phase(to_insert.get_phase());
}

void Circuit::substitute(
    const Circuit& to_insert, const Vertex& to_replace,
    VertexDeletion vertex_deletion, OpGroupTransfer opgroup_transfer) {
  Subcircuit sub = singleton_subcircuit(to_replace);
  substitute(to_insert, sub, vertex_deletion, opgroup_transfer);
}

// Helper function for substitute_conditional which recursively unpacks a
// Conditional until we reach something that isn't a Conditional; we then wrap
// base_circ with each layer of the conditional working back up
Circuit recursive_conditional_circuit(
    const Op_ptr& op, const Circuit& base_circ) {
  if (op->get_type() != OpType::Conditional) return base_circ;
  const Conditional& cond = static_cast<const Conditional&>(*op);
  Op_ptr inner_op = cond.get_op();
  Circuit inner_circ = recursive_conditional_circuit(inner_op, base_circ);
  unsigned width = cond.get_width();
  bit_map_t rename_map;
  for (unsigned i = 0; i < inner_circ.n_bits(); ++i)
    rename_map[Bit(i)] = Bit(i + width);
  inner_circ.rename_units(rename_map);
  bit_vector_t cond_bits(width);
  for (unsigned i = 0; i < width; ++i) cond_bits[i] = Bit(i);
  return inner_circ.conditional_circuit(cond_bits, cond.get_value());
}

void Circuit::substitute_conditional(
    Circuit to_insert, const Vertex& to_replace, VertexDeletion vertex_deletion,
    OpGroupTransfer opgroup_transfer) {
  Op_ptr op = get_Op_ptr_from_Vertex(to_replace);
  if (op->get_type() != OpType::Conditional)
    throw CircuitInvalidity(
        "substitute_conditional called with an unconditional gate");
  Subcircuit sub = singleton_subcircuit(to_replace);
  to_insert = recursive_conditional_circuit(op, to_insert);
  substitute(to_insert, sub, vertex_deletion, opgroup_transfer);
}

// given the edges to be broken and new
// circuit, implants circuit into old circuit
void Circuit::cut_insert(
    const Circuit& incirc, const EdgeVec& preds, const EdgeVec& b_future) {
  std::vector<std::optional<Edge>> succs{preds.begin(), preds.end()};
  Subcircuit sub = {preds, succs, b_future, {}};
  substitute(incirc, sub, VertexDeletion::No);
}

bool Circuit::replace_SWAPs(bool replace_tk2_equivalents) {
  VertexList bin;
  bool changed = false;
  double total_phase = 0.;
  BGL_FORALL_VERTICES(v, dag, DAG) {
    Op_ptr op = get_Op_ptr_from_Vertex(v);
    OpType type = op->get_type();
    if (type != OpType::SWAP &&
        (type != OpType::TK2 || !replace_tk2_equivalents)) {
      continue;
    }
    if (type == OpType::TK2) {
      std::vector<Expr> params = op->get_params();
      std::optional<double> phase =
          is_TK2_SWAP(params[0], params[1], params[2]);
      if (phase == std::nullopt) {
        continue;
      }
      total_phase += *phase;
    }
    Vertex swap = v;
    EdgeVec outs = get_all_out_edges(v);
    Edge out1 = outs[0];
    dag[out1].ports.first = 1;
    Edge out2 = outs[1];
    dag[out2].ports.first = 0;
    remove_vertex(swap, GraphRewiring::Yes, VertexDeletion::No);
    bin.push_back(swap);
    changed = true;
  }
  remove_vertices(bin, GraphRewiring::No, VertexDeletion::Yes);
  add_phase(total_phase);
  return changed;
}

void Circuit::replace_implicit_wire_swap(
    const Qubit first, const Qubit second, bool using_cx) {
  Vertex last_v;
  if (using_cx) {
    add_op<UnitID>(OpType::CX, {first, second});
    add_op<UnitID>(OpType::CX, {second, first});
    last_v = add_op<UnitID>(OpType::CX, {first, second});
  } else {
    last_v = add_op<UnitID>(OpType::SWAP, {first, second});
  }
  EdgeVec outs = get_all_out_edges(last_v);
  Edge out1 = outs[0];
  dag[out1].ports.first = 1;
  Edge out2 = outs[1];
  dag[out2].ports.first = 0;
}

void Circuit::replace_all_implicit_wire_swaps() {
  qubit_map_t perm = implicit_qubit_permutation();
  std::set<Qubit> fixed_qubits;
  // iterate permutation cycles and add swap for every adjacent elements
  for (const std::pair<const Qubit, Qubit>& pair : perm) {
    if (fixed_qubits.find(pair.first) != fixed_qubits.end()) {
      // skip if visited
      continue;
    }
    // start traverse a cycle
    const Qubit head = pair.first;
    Qubit current = pair.first;
    Qubit next = pair.second;
    while (true) {
      if (next == head) {
        // break if reaches the end of the cycle
        fixed_qubits.insert(current);
        break;
      }
      replace_implicit_wire_swap(current, next, false);
      fixed_qubits.insert(current);
      auto it = perm.find(next);
      TKET_ASSERT(it != perm.end());
      current = it->first;
      next = it->second;
    }
  }
}

// helper functions for the dagger and transpose
void Circuit::_handle_boundaries(Circuit& circ, vertex_map_t& vmap) const {
  // Handle boundaries
  for (const BoundaryElement& el : this->boundary.get<TagID>()) {
    Vertex new_in;
    Vertex new_out;
    if (el.id_.type() == UnitType::Bit) {
      tket_log()->warn(
          "The circuit contains classical data for which the dagger/transpose "
          "might not be defined.");
      new_in = circ.add_vertex(OpType::ClInput);
      new_out = circ.add_vertex(OpType::ClOutput);
    } else {
      new_in = circ.add_vertex(OpType::Input);
      new_out = circ.add_vertex(OpType::Output);
    }
    Vertex old_in = el.in_;
    Vertex old_out = el.out_;
    vmap[old_in] = new_out;
    vmap[old_out] = new_in;
    circ.boundary.insert({el.id_, new_in, new_out});
  }
}

void Circuit::_handle_interior(
    Circuit& circ, vertex_map_t& vmap, V_iterator& vi, V_iterator& vend,
    ReverseType reverse_op) const {
  // Handle interior
  for (std::tie(vi, vend) = boost::vertices(this->dag); vi != vend; vi++) {
    const Op_ptr op = get_Op_ptr_from_Vertex(*vi);
    OpDesc desc = op->get_desc();
    if (is_boundary_q_type(desc.type())) {
      continue;
    } else if ((desc.is_gate() || desc.is_box()) && !desc.is_oneway()) {
      Op_ptr op_type_ptr;
      switch (reverse_op) {
        case ReverseType::dagger: {
          op_type_ptr = op->dagger();
          break;
        }
        case ReverseType::transpose: {
          op_type_ptr = op->transpose();
          break;
        }
        default: {
          throw std::logic_error(
              "Error in the definition of the dagger or transpose.");
        }
      }
      Vertex v = circ.add_vertex(op_type_ptr);
      vmap[*vi] = v;
    } else if (desc.is_barrier()) {
      Vertex v = circ.add_vertex(op);
      vmap[*vi] = v;
    } else {
      throw CircuitInvalidity(
          "Cannot dagger or transpose op: " + op->get_name());
    }
  }
}

void Circuit::_handle_edges(
    Circuit& circ, vertex_map_t& vmap, E_iterator& ei, E_iterator& eend) const {
  for (std::tie(ei, eend) = boost::edges(this->dag); ei != eend; ei++) {
    Vertex s = source(*ei);
    port_t sp = get_source_port(*ei);
    Vertex t = target(*ei);
    port_t tp = get_target_port(*ei);
    circ.add_edge({vmap[t], tp}, {vmap[s], sp}, get_edgetype(*ei));
  }
}

// returns Hermitian conjugate of circuit, ie its inverse
Circuit Circuit::dagger() const {
  Circuit c;
  vertex_map_t vmap;
  _handle_boundaries(c, vmap);

  V_iterator vi, vend;
  ReverseType dagger = ReverseType::dagger;
  _handle_interior(c, vmap, vi, vend, dagger);

  E_iterator ei, eend;
  _handle_edges(c, vmap, ei, eend);

  c.add_phase(-get_phase());
  return c;
}

// returns transpose of circuit
Circuit Circuit::transpose() const {
  Circuit c;
  vertex_map_t vmap;
  _handle_boundaries(c, vmap);

  V_iterator vi, vend;
  ReverseType transpose = ReverseType::transpose;
  _handle_interior(c, vmap, vi, vend, transpose);

  E_iterator ei, eend;
  _handle_edges(c, vmap, ei, eend);

  c.add_phase(get_phase());
  return c;
}

bool Circuit::substitute_all(const Circuit& to_insert, const Op_ptr op) {
  if (!to_insert.is_simple()) throw SimpleOnly();
  if (op->n_qubits() != to_insert.n_qubits())
    throw CircuitInvalidity(
        "Cannot substitute all on mismatching arity between Vertex "
        "and inserted Circuit");
  VertexVec to_replace;
  VertexVec conditional_to_replace;
  BGL_FORALL_VERTICES(v, dag, DAG) {
    Op_ptr v_op = get_Op_ptr_from_Vertex(v);
    if (*v_op == *op)
      to_replace.push_back(v);
    else if (v_op->get_type() == OpType::Conditional) {
      while (v_op->get_type() == OpType::Conditional) {
        v_op = static_cast<const Conditional&>(*v_op).get_op();
      }
      if (*v_op == *op) conditional_to_replace.push_back(v);
    }
  }
  for (const Vertex& v : to_replace) {
    substitute(to_insert, v, VertexDeletion::Yes);
  }
  for (const Vertex& v : conditional_to_replace) {
    substitute_conditional(to_insert, v, VertexDeletion::Yes);
  }
  return !(to_replace.empty() && conditional_to_replace.empty());
}

bool Circuit::substitute_named(
    const Circuit& to_insert, const std::string opname) {
  if (!to_insert.is_simple()) throw SimpleOnly();

  // Check that no op group names are in common
  for (const auto& opgroupsig : to_insert.opgroupsigs) {
    if (opgroupsigs.find(opgroupsig.first) != opgroupsigs.end()) {
      throw CircuitInvalidity("Name collision in replacement circuit");
    }
  }

  // Do nothing if opname not present
  if (opgroupsigs.find(opname) == opgroupsigs.end()) {
    return false;
  }

  // Check signatures match
  op_signature_t sig = opgroupsigs[opname];
  unsigned sig_n_q = std::count(sig.begin(), sig.end(), EdgeType::Quantum);
  unsigned sig_n_c = std::count(sig.begin(), sig.end(), EdgeType::Classical);
  unsigned sig_n_b = std::count(sig.begin(), sig.end(), EdgeType::Boolean);
  if (to_insert.n_qubits() != sig_n_q || to_insert.n_bits() != sig_n_c ||
      sig_n_b != 0) {
    throw CircuitInvalidity("Signature mismatch");
  }

  VertexVec to_replace;
  BGL_FORALL_VERTICES(v, dag, DAG) {
    std::optional<std::string> v_opgroup = get_opgroup_from_Vertex(v);
    if (v_opgroup && v_opgroup.value() == opname) {
      to_replace.push_back(v);
    }
  }

  for (const Vertex& v : to_replace) {
    substitute(to_insert, v, VertexDeletion::Yes, OpGroupTransfer::Merge);
  }

  return !to_replace.empty();
}

bool Circuit::substitute_named(Op_ptr to_insert, const std::string opname) {
  // Do nothing if opname not present
  if (opgroupsigs.find(opname) == opgroupsigs.end()) {
    return false;
  }

  // Check signatures match
  op_signature_t sig = opgroupsigs[opname];
  if (to_insert->get_signature() != sig) {
    throw CircuitInvalidity("Signature mismatch");
  }

  VertexVec to_replace;
  BGL_FORALL_VERTICES(v, dag, DAG) {
    std::optional<std::string> v_opgroup = get_opgroup_from_Vertex(v);
    if (v_opgroup && v_opgroup.value() == opname) {
      to_replace.push_back(v);
    }
  }

  unsigned sig_n_q = std::count(sig.begin(), sig.end(), EdgeType::Quantum);
  unsigned sig_n_c = std::count(sig.begin(), sig.end(), EdgeType::Classical);
  Circuit c(sig_n_q, sig_n_c);
  unit_vector_t args(sig_n_q + sig_n_c);
  for (unsigned i = 0; i < sig_n_q; i++) args[i] = Qubit(i);
  for (unsigned i = 0; i < sig_n_c; i++) args[sig_n_q + i] = Bit(i);
  c.add_op(to_insert, args, opname);
  for (const Vertex& v : to_replace) {
    substitute(c, v, VertexDeletion::Yes, OpGroupTransfer::Merge);
  }

  return !to_replace.empty();
}

Circuit Circuit::conditional_circuit(
    const bit_vector_t& bits, unsigned value) const {
  if (has_implicit_wireswaps()) {
    throw CircuitInvalidity("Cannot add conditions to an implicit wireswap");
  }
  Circuit cond_circ(all_qubits(), all_bits());
  for (const Bit& b : bits) {
    if (contains_unit(b)) {
      Vertex in = get_in(b);
      Vertex out = get_out(b);
      if (get_successors_of_type(in, EdgeType::Classical).front() != out) {
        throw CircuitInvalidity(
            "Cannot add condition. Circuit has non-trivial "
            "actions on bit " +
            b.repr());
      }
    } else {
      cond_circ.add_bit(b);
    }
  }
  unsigned width = bits.size();
  for (const Command& com : *this) {
    const Op_ptr op = com.get_op_ptr();
    Op_ptr cond_op = std::make_shared<Conditional>(op, width, value);
    unit_vector_t args = com.get_args();
    args.insert(args.begin(), bits.begin(), bits.end());
    cond_circ.add_op(cond_op, args);
  }
  // Replace global phase with conditional phase:
  Expr alpha = get_phase();
  if (!equiv_0(alpha)) {
    Op_ptr op = get_op_ptr(OpType::Phase, {alpha});
    Op_ptr cond_op = std::make_shared<Conditional>(op, width, value);
    cond_circ.add_op(cond_op, bits);
  }
  return cond_circ;
}

bool Circuit::substitute_box_vertex(
    Vertex& vert, VertexDeletion vertex_deletion,
    const std::unordered_set<OpType>& excluded_types,
    const std::unordered_set<std::string>& excluded_opgroups) {
  Op_ptr op = get_Op_ptr_from_Vertex(vert);
  bool conditional = false;
  while (op->get_type() == OpType::Conditional) {
    op = static_cast<const Conditional&>(*op).get_op();
    conditional = true;
  }
  if (!op->get_desc().is_box()) return false;
  const Box& b = static_cast<const Box&>(*op);
  Circuit replacement = *b.to_circuit();
  replacement.decompose_boxes_recursively(excluded_types, excluded_opgroups);
  replacement.flatten_registers();
  if (conditional) {
    substitute_conditional(
        replacement, vert, vertex_deletion, OpGroupTransfer::Merge);
  } else {
    substitute(replacement, vert, vertex_deletion, OpGroupTransfer::Merge);
  }
  return true;
}

bool Circuit::decompose_boxes_recursively(
    const std::unordered_set<OpType>& excluded_types,
    const std::unordered_set<std::string>& excluded_opgroups,
    const std::optional<std::unordered_set<OpType>>& included_types,
    const std::optional<std::unordered_set<std::string>>& included_opgroups) {
  bool success = false;
  VertexList bin;
  BGL_FORALL_VERTICES(v, dag, DAG) {
    OpType ot = get_OpType_from_Vertex(v);
    if (excluded_types.contains(ot)) continue;
    if (included_types && !included_types->contains(ot)) continue;
    std::optional<std::string> v_opgroup = get_opgroup_from_Vertex(v);
    if (v_opgroup && excluded_opgroups.contains(v_opgroup.value())) continue;
    if (included_opgroups &&
        (!v_opgroup || !included_opgroups->contains(v_opgroup.value())))
      continue;
    if (substitute_box_vertex(
            v, VertexDeletion::No, excluded_types, excluded_opgroups)) {
      bin.push_back(v);
      success = true;
    }
  }
  remove_vertices(bin, GraphRewiring::No, VertexDeletion::Yes);
  return success;
}

std::map<Bit, bool> Circuit::classical_eval(
    const std::map<Bit, bool>& values) const {
  std::map<Bit, bool> v(values);
  for (CommandIterator it = begin(); it != end(); ++it) {
    Op_ptr op = it->get_op_ptr();
    OpType optype = op->get_type();
    if (!is_classical_type(optype)) {
      throw CircuitInvalidity("Non-classical operation");
    }
    std::shared_ptr<const ClassicalEvalOp> cop =
        std::dynamic_pointer_cast<const ClassicalEvalOp>(op);
    unit_vector_t args = it->get_args();
    unsigned n_args = args.size();
    switch (optype) {
      case OpType::ClassicalTransform: {
        std::vector<bool> input(n_args);
        for (unsigned i = 0; i < n_args; i++) {
          input[i] = v[Bit(args[i])];
        }
        std::vector<bool> output = cop->eval(input);
        TKET_ASSERT(output.size() == n_args);
        for (unsigned i = 0; i < n_args; i++) {
          v[Bit(args[i])] = output[i];
        }
        break;
      }
      case OpType::SetBits: {
        std::vector<bool> output = cop->eval({});
        TKET_ASSERT(output.size() == n_args);
        for (unsigned i = 0; i < n_args; i++) {
          v[Bit(args[i])] = output[i];
        }
        break;
      }
      default:
        throw CircuitInvalidity("Unexpected operation in circuit");
    }
  }
  return v;
}

}  // namespace tket
