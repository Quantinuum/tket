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

#include "tket/Converters/Converters.hpp"
#include "tket/ZX/Flow.hpp"
#include "tket/ZX/ZXDiagram.hpp"

namespace tket {

using namespace zx;

enum class ZXPortType { In, Out };
typedef std::pair<VertPort, ZXPortType> TypedVertPort;
typedef std::pair<ZXVert, std::optional<unsigned>> ZXVertPort;
typedef std::vector<ZXVertPort> ZXVertPortVec;
typedef boost::bimap<ZXVert, Vertex> BoundaryVertMap;

bool is_spiderless_optype(const OpType& optype) {
  return optype == OpType::Barrier || optype == OpType::SWAP ||
         optype == OpType::noop;
}

// Add a switch with the on-state controlled by the on_value.
// qtype indicates the quantum type of the switch.
std::pair<ZXVertPort, ZXVertPort> add_switch(
    ZXDiagram& zxd, const bool& on_value, const QuantumType& qtype) {
  zxd.multiply_scalar(std::sqrt(2.));
  ZXVert triangle = zxd.add_vertex(ZXType::Triangle, qtype);
  ZXVert x = zxd.add_vertex(ZXType::XSpider, 0, qtype);
  zxd.add_wire(triangle, x, ZXWireType::Basic, qtype, 1);
  if (on_value) {
    // Turn on the switch if the input is 1
    ZXVert negate = zxd.add_vertex(ZXType::XSpider, 1, qtype);
    zxd.add_wire(triangle, negate, ZXWireType::Basic, qtype, 0);
    return {{negate, std::nullopt}, {x, std::nullopt}};
  }
  return {{triangle, 0}, {x, std::nullopt}};
}

// --o--s---o--
//   |      |
//   n      n
//   |--G-o-|
//        |
//        s
//        |-discard
// s, n are switches, G is the conditional zx diagram specified by 'left' and
// 'right'. s is on when the input is 0, n is on when the input is 1.
// return the input and the output of the conditional zx along with a vector of
// control spiders
std::pair<std::pair<ZXVertPort, ZXVertPort>, ZXVertPortVec> add_conditional_zx(
    ZXDiagram& zxd, const ZXVert& left, const ZXVert& right,
    const QuantumType& qtype) {
  ZXVert in = zxd.add_vertex(ZXType::ZSpider, 0, qtype);
  ZXVert out = zxd.add_vertex(ZXType::ZSpider, 0, qtype);
  ZXVert discard_ctr = zxd.add_vertex(ZXType::ZSpider, 0, qtype);
  ZXVert discard = zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Classical);
  auto switch_s0 = add_switch(zxd, false, qtype);
  auto switch_n0 = add_switch(zxd, true, qtype);
  auto switch_n1 = add_switch(zxd, true, qtype);
  auto switch_s1 = add_switch(zxd, false, qtype);
  zxd.add_wire(in, switch_s0.second.first, ZXWireType::Basic, qtype);
  zxd.add_wire(out, switch_s0.second.first, ZXWireType::Basic, qtype);
  zxd.add_wire(in, switch_n0.second.first, ZXWireType::Basic, qtype);
  zxd.add_wire(left, switch_n0.second.first, ZXWireType::Basic, qtype);
  zxd.add_wire(right, discard_ctr, ZXWireType::Basic, qtype);
  zxd.add_wire(discard_ctr, switch_n1.second.first, ZXWireType::Basic, qtype);
  zxd.add_wire(out, switch_n1.second.first, ZXWireType::Basic, qtype);
  zxd.add_wire(discard_ctr, switch_s1.second.first, ZXWireType::Basic, qtype);
  zxd.add_wire(discard, switch_s1.second.first, ZXWireType::Basic, qtype);
  return {
      {{in, std::nullopt}, {out, std::nullopt}},
      {switch_s0.first, switch_s1.first, switch_n0.first, switch_n1.first}};
}

// n-bit AND spider to control the switches
// https://arxiv.org/abs/1910.06818
std::pair<ZXVertPortVec, ZXVertPort> add_n_bit_and(
    ZXDiagram& zxd, unsigned n, const QuantumType& qtype) {
  TKET_ASSERT(n > 1);
  ZXVert z_vert = zxd.add_vertex(ZXType::ZSpider, 0, qtype);
  // Add Triangle -1
  ZXVert z_pi_0 = zxd.add_vertex(ZXType::ZSpider, 1, qtype);
  ZXVert tri_1 = zxd.add_vertex(ZXType::Triangle, qtype);
  ZXVert z_pi_1 = zxd.add_vertex(ZXType::ZSpider, 1, qtype);
  zxd.add_wire(z_vert, z_pi_0, ZXWireType::Basic, qtype);
  zxd.add_wire(tri_1, z_pi_0, ZXWireType::Basic, qtype, 0);
  zxd.add_wire(tri_1, z_pi_1, ZXWireType::Basic, qtype, 1);
  ZXVertPortVec inputs;
  for (unsigned i = 0; i < n; i++) {
    ZXVert tri_0 = zxd.add_vertex(ZXType::Triangle, qtype);
    zxd.add_wire(tri_0, z_vert, ZXWireType::Basic, qtype, 1);
    inputs.push_back({tri_0, 0});
  }
  return {inputs, {z_pi_1, std::nullopt}};
}

// Add converted circ into zxd. Set add_boundary to true
// if boundary spiders are to be added to the boundary.
BoundaryVertMap circuit_to_zx_recursive(
    const Circuit& circ, ZXDiagram& zxd, bool add_boundary) {
  std::map<TypedVertPort, ZXVertPort> vert_lookup;
  std::map<VertPort, ZXVertPort> boolean_outport_lookup;
  BoundaryVertMap bmap;

  // Convert each vertex to ZXDiagram, raise error if not supported
  BGL_FORALL_VERTICES(vert, circ.dag, DAG) {
    // We currently throw an error if the vertex is either classical or flow
    Op_ptr op = circ.get_Op_ptr_from_Vertex(vert);
    if (is_flowop_type(op->get_type()) || is_classical_type(op->get_type())) {
      throw Unsupported(
          "Cannot convert OpType: " + op->get_name() + " to a ZX node. \n");
    }
    switch (op->get_type()) {
      case OpType::Input: {
        ZXVert zx_vert = zxd.add_vertex(ZXType::Input, QuantumType::Quantum);
        if (add_boundary) zxd.add_boundary(zx_vert);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_vert, std::nullopt}});
        bmap.insert({zx_vert, vert});
        break;
      }
      case OpType::Output: {
        ZXVert zx_vert = zxd.add_vertex(ZXType::Output, QuantumType::Quantum);
        if (add_boundary) zxd.add_boundary(zx_vert);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_vert, std::nullopt}});
        bmap.insert({zx_vert, vert});
        break;
      }
      case OpType::ClInput: {
        ZXVert zx_vert = zxd.add_vertex(ZXType::Input, QuantumType::Classical);
        if (add_boundary) zxd.add_boundary(zx_vert);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_vert, std::nullopt}});
        bmap.insert({zx_vert, vert});
        break;
      }
      case OpType::ClOutput: {
        ZXVert zx_vert = zxd.add_vertex(ZXType::Output, QuantumType::Classical);
        if (add_boundary) zxd.add_boundary(zx_vert);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_vert, std::nullopt}});
        bmap.insert({zx_vert, vert});
        break;
      }
      // Spiderless ops are handled during vertex wiring
      case OpType::Barrier:
      case OpType::noop:
      case OpType::SWAP:
      case OpType::Phase: {
        continue;
      }
      case OpType::H: {
        ZXVert zx_vert = zxd.add_vertex(ZXType::Hbox, QuantumType::Quantum);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_vert, std::nullopt}});
        zxd.multiply_scalar(0.5);
        break;
      }
      case OpType::Rz: {
        ZXVert zx_vert = zxd.add_vertex(
            ZXType::ZSpider, op->get_params()[0], QuantumType::Quantum);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_vert, std::nullopt}});
        break;
      }
      case OpType::Rx: {
        ZXVert zx_vert = zxd.add_vertex(
            ZXType::XSpider, op->get_params()[0], QuantumType::Quantum);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_vert, std::nullopt}});
        break;
      }
      case OpType::X: {
        ZXVert zx_vert =
            zxd.add_vertex(ZXType::XSpider, 1, QuantumType::Quantum);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_vert, std::nullopt}});
        break;
      }
      case OpType::Z: {
        ZXVert zx_vert =
            zxd.add_vertex(ZXType::ZSpider, 1, QuantumType::Quantum);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_vert, std::nullopt}});
        break;
      }
      case OpType::CX: {
        ZXVert zx_x_vert =
            zxd.add_vertex(ZXType::XSpider, 0, QuantumType::Quantum);
        ZXVert zx_z_vert =
            zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Quantum);
        zxd.add_wire(zx_x_vert, zx_z_vert);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_z_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_z_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 1}, ZXPortType::In}, {zx_x_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 1}, ZXPortType::Out}, {zx_x_vert, std::nullopt}});
        zxd.multiply_scalar(2);
        break;
      }
      case OpType::CZ: {
        ZXVert zx_za_vert =
            zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Quantum);
        ZXVert zx_zb_vert =
            zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Quantum);
        zxd.add_wire(zx_za_vert, zx_zb_vert, ZXWireType::H);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_za_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_za_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 1}, ZXPortType::In}, {zx_zb_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 1}, ZXPortType::Out}, {zx_zb_vert, std::nullopt}});
        break;
      }
      case OpType::Measure: {
        // Add a decoherence node
        ZXVert zx_measure_vert =
            zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Classical);
        // Add a delete operator
        ZXVert zx_delete_vert =
            zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Classical);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_measure_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_measure_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 1}, ZXPortType::In}, {zx_delete_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 1}, ZXPortType::Out}, {zx_measure_vert, std::nullopt}});
        break;
      }
      case OpType::Reset: {
        // Discard
        ZXVert zx_discard_vert =
            zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Classical);
        // Add a node to prepare |0>
        ZXVert zx_reset_vert =
            zxd.add_vertex(ZXType::XSpider, 0, QuantumType::Quantum);
        zxd.multiply_scalar(0.5);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_discard_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_reset_vert, std::nullopt}});
        break;
      }
      case OpType::Collapse: {
        ZXVert zx_vert =
            zxd.add_vertex(ZXType::ZSpider, QuantumType::Classical);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_vert, std::nullopt}});
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_vert, std::nullopt}});
        break;
      }
      case OpType::Create: {
        ZXVert zx_init_vert =
            zxd.add_vertex(ZXType::XSpider, 0, QuantumType::Quantum);
        zxd.multiply_scalar(0.5);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::Out}, {zx_init_vert, std::nullopt}});
        break;
      }
      case OpType::Discard: {
        ZXVert zx_discard_vert =
            zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Classical);
        vert_lookup.insert(
            {{{vert, 0}, ZXPortType::In}, {zx_discard_vert, std::nullopt}});
        break;
      }
      case OpType::Conditional: {
        // Stores the condition for each port index
        std::vector<bool> port_conditions;

        op_signature_t parent_sig = op->get_signature();

        // Find the nested quantum op and its overall conditions
        // Assume bool ports are always the first few ports
        while (op->get_type() == OpType::Conditional) {
          const Conditional& cond = static_cast<const Conditional&>(*op);
          for (unsigned i = 0; i < cond.get_width(); i++) {
            bool set = cond.get_value() & (1 << (cond.get_width() - i - 1));
            port_conditions.push_back(set);
          }
          op = cond.get_op();
        }
        if (is_barrier_type(op->get_type())) {
          throw Unsupported(
              "Cannot convert conditional barrier operations to a ZX node. \n");
        }

        unsigned port_conditions_size =
            static_cast<unsigned>(port_conditions.size());
        // Convert the underlying op to zx.
        // If the op is a quantum gate, construct a 1 gate circuit
        // If the op is a box, obtain its circuit decomposition
        op_signature_t inner_sig = op->get_signature();
        Circuit replacement;
        if (is_box_type(op->get_type())) {
          const Box& b = static_cast<const Box&>(*op);
          replacement = *b.to_circuit();
        } else {
          unit_vector_t args;
          unsigned q_index = 0;
          unsigned c_index = 0;
          for (const EdgeType& etype : inner_sig) {
            if (etype == EdgeType::Quantum) {
              args.push_back(Qubit(q_index++));
            } else if (etype == EdgeType::Classical) {
              args.push_back(Bit(c_index++));
            }
            TKET_ASSERT(etype != EdgeType::Boolean);
          }
          replacement = Circuit(q_index, c_index);
          replacement.add_op(op, args);
        }
        BoundaryVertMap box_bm =
            circuit_to_zx_recursive(replacement, zxd, false);

        // The Z spider controlling all switches
        ZXVert master_switch =
            zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Classical);

        // For each qubit/bit path in the inner op, convert it into a
        // conditional path.
        unsigned q_index = 0;
        unsigned c_index = 0;
        for (unsigned i = 0; i < inner_sig.size(); i++) {
          QuantumType qtype;
          UnitID reg;
          if (inner_sig[i] == EdgeType::Quantum) {
            qtype = QuantumType::Quantum;
            reg = Qubit(q_index++);
          } else {
            qtype = QuantumType::Classical;
            reg = Bit(c_index++);
          }
          std::pair<ZXVertPort, ZXVertPort> boundary;
          ZXVertPortVec controls;
          Vertex inp = replacement.get_in(reg);
          ZXVert zx_inp = box_bm.right.at(inp);
          Vertex outp = replacement.get_out(reg);
          ZXVert zx_outp = box_bm.right.at(outp);
          // Convert the path between zx_inp and zx_outp into a conditional
          // path
          std::tie(boundary, controls) =
              add_conditional_zx(zxd, zx_inp, zx_outp, qtype);
          // Connect each switch to the master switch
          for (const ZXVertPort& ctr : controls) {
            zxd.add_wire(
                ctr.first, master_switch, ZXWireType::Basic,
                zxd.get_qtype(ctr.first).value(), ctr.second);
          }
          // Update lookup
          TKET_ASSERT(parent_sig[i + port_conditions_size] == inner_sig[i]);
          // Since we assume that the conditions always use the first few
          // ports, the gate path should use the i + port_conditions.size()
          // port.
          vert_lookup.insert(
              {{{vert, i + port_conditions_size}, ZXPortType::In},
               boundary.first});
          vert_lookup.insert(
              {{{vert, i + port_conditions_size}, ZXPortType::Out},
               boundary.second});
        }
        // Use either a Z spider or a AND operator to connect the master
        // switch and the boolean inputs
        ZXVertPortVec and_inputs;
        if (port_conditions_size == 1) {
          and_inputs.push_back({master_switch, std::nullopt});
        } else {
          ZXVertPort and_output;
          std::tie(and_inputs, and_output) =
              add_n_bit_and(zxd, port_conditions_size, QuantumType::Classical);
          zxd.add_wire(
              and_output.first, master_switch, ZXWireType::Basic,
              QuantumType::Classical, and_output.second);
        }
        // Connect inputs to the nodes obtained from above
        for (unsigned i = 0; i < port_conditions_size; i++) {
          // Each boolean edge shares a source port with other
          // classical/boolean edges. Use the classical Z spider to explicitly
          // implement this copy operation
          Edge in_edge = circ.get_nth_in_edge(vert, i);
          port_t source_port = circ.get_source_port(in_edge);
          Vertex vert_s = circ.source(in_edge);
          // During the wiring stage, each edge originated from {vert_s,
          // p_s} needs go through copy_vert
          if (boolean_outport_lookup.find({vert_s, source_port}) ==
              boolean_outport_lookup.end()) {
            ZXVert copy_vert =
                zxd.add_vertex(ZXType::ZSpider, 0, QuantumType::Classical);
            boolean_outport_lookup.insert(
                {{vert_s, source_port}, {copy_vert, std::nullopt}});
          }
          if (port_conditions[i]) {
            vert_lookup.insert({{{vert, i}, ZXPortType::In}, and_inputs[i]});
          } else {
            ZXVert x_vert =
                zxd.add_vertex(ZXType::XSpider, 1, QuantumType::Classical);
            zxd.add_wire(
                and_inputs[i].first, x_vert, ZXWireType::Basic,
                QuantumType::Classical, and_inputs[i].second);
            vert_lookup.insert(
                {{{vert, i}, ZXPortType::In}, {x_vert, std::nullopt}});
          }
        }
        break;
      }
      default:
        if (is_box_type(op->get_type())) {
          EdgeVec b_in_holes =
              circ.get_in_edges_of_type(vert, EdgeType::Boolean);
          if (b_in_holes.size() > 0) {
            throw Unsupported("Cannot convert box type: " + op->get_name());
          }
          const Box& b = static_cast<const Box&>(*op);
          Circuit replacement = *b.to_circuit();
          // First, add the converted box to diagram.
          BoundaryVertMap box_bm =
              circuit_to_zx_recursive(replacement, zxd, false);
          // Then, map the vertports in the box boundary to zx nodes.
          // Assume that a box can't have Boolean input edges, and all Boolean
          // output edges share ports with Classical edges. Therefore we don't
          // have to map Boolean vertports.
          EdgeVec q_in_holes =
              circ.get_in_edges_of_type(vert, EdgeType::Quantum);
          EdgeVec q_out_holes =
              circ.get_out_edges_of_type(vert, EdgeType::Quantum);
          EdgeVec c_in_holes =
              circ.get_in_edges_of_type(vert, EdgeType::Classical);
          EdgeVec c_out_holes =
              circ.get_out_edges_of_type(vert, EdgeType::Classical);

          for (unsigned i = 0; i < q_in_holes.size(); i++) {
            port_t port = circ.get_target_port(q_in_holes[i]);
            Vertex inp = replacement.get_in(Qubit(i));
            vert_lookup.insert(
                {{{vert, port}, ZXPortType::In},
                 {box_bm.right.at(inp), std::nullopt}});
          }
          for (unsigned i = 0; i < q_out_holes.size(); i++) {
            port_t port = circ.get_source_port(q_out_holes[i]);
            Vertex outp = replacement.get_out(Qubit(i));
            vert_lookup.insert(
                {{{vert, port}, ZXPortType::Out},
                 {box_bm.right.at(outp), std::nullopt}});
          }
          for (unsigned i = 0; i < c_in_holes.size(); i++) {
            port_t port = circ.get_target_port(c_in_holes[i]);
            Vertex inp = replacement.get_in(Bit(i));
            vert_lookup.insert(
                {{{vert, port}, ZXPortType::In},
                 {box_bm.right.at(inp), std::nullopt}});
          }
          for (unsigned i = 0; i < c_out_holes.size(); i++) {
            port_t port = circ.get_source_port(c_out_holes[i]);
            Vertex outp = replacement.get_out(Bit(i));
            vert_lookup.insert(
                {{{vert, port}, ZXPortType::Out},
                 {box_bm.right.at(outp), std::nullopt}});
          }
        } else {
          throw Unsupported(
              "Cannot convert gate type: " + op->get_name() +
              " to a ZX node, try rebase the gates to use Rx, Rz, "
              "X, Z, H, CZ "
              "or CX. \n");
        }
    }
  }

  // Connect the ZX nodes
  BGL_FORALL_EDGES(edge, circ.dag, DAG) {
    Vertex v_s = circ.source(edge);
    Vertex v_t = circ.target(edge);
    port_t p_s = circ.get_source_port(edge);
    port_t p_t = circ.get_target_port(edge);
    // Handle Spiderless ops
    if (is_spiderless_optype(circ.get_OpType_from_Vertex(v_s))) {
      // We only handle in-edges
      continue;
    }
    if (is_spiderless_optype(circ.get_OpType_from_Vertex(v_t))) {
      // Traverse the path to find the next non-spiderless op
      Edge next_e = edge;
      do {
        if (circ.get_OpType_from_Vertex(v_t) == OpType::SWAP) {
          next_e = circ.get_nth_out_edge(
              v_t, (circ.get_target_port(next_e) + 1) % 2);
          v_t = circ.target(next_e);
        } else {
          std::tie(v_t, next_e) = circ.get_next_pair(v_t, next_e);
        }
      } while (is_spiderless_optype(circ.get_OpType_from_Vertex(v_t)));
      p_t = circ.get_target_port(next_e);
    }

    ZXVertPort zx_vp_s =
        vert_lookup.at(TypedVertPort(VertPort(v_s, p_s), ZXPortType::Out));
    ZXVertPort zx_vp_t =
        vert_lookup.at(TypedVertPort(VertPort(v_t, p_t), ZXPortType::In));
    if (circ.get_edgetype(edge) == EdgeType::Quantum) {
      zxd.add_wire(
          zx_vp_s.first, zx_vp_t.first, ZXWireType::Basic, QuantumType::Quantum,
          zx_vp_s.second, zx_vp_t.second);
    } else {
      auto bool_it = boolean_outport_lookup.find(VertPort(v_s, p_s));
      if (bool_it == boolean_outport_lookup.end()) {
        zxd.add_wire(
            zx_vp_s.first, zx_vp_t.first, ZXWireType::Basic,
            QuantumType::Classical, zx_vp_s.second, zx_vp_t.second);
      } else {
        // If the source port is boolean, then connect the copy spider to the
        // source vertex. All out-edges originated from the source port should
        // be connected to the copy spider.
        if (zxd.degree(bool_it->second.first) == 0) {
          zxd.add_wire(
              zx_vp_s.first, bool_it->second.first, ZXWireType::Basic,
              QuantumType::Classical, zx_vp_s.second, bool_it->second.second);
        }
        zxd.add_wire(
            bool_it->second.first, zx_vp_t.first, ZXWireType::Basic,
            QuantumType::Classical, bool_it->second.second, zx_vp_t.second);
      }
    }
  }
  return bmap;
}

std::pair<ZXDiagram, BoundaryVertMap> circuit_to_zx(const Circuit& circ) {
  ZXDiagram zxd;
  BoundaryVertMap bmap = circuit_to_zx_recursive(circ, zxd, true);
  // Remove internal boundary vertices produced by the recursion
  ZXVertVec true_boundary = zxd.get_boundary();
  ZXVertIterator vi, vi_end, next;
  tie(vi, vi_end) = boost::vertices(*zxd.get_graph());
  for (next = vi; vi != vi_end; vi = next) {
    ++next;
    if ((zxd.get_zxtype(*vi) == ZXType::Input ||
         zxd.get_zxtype(*vi) == ZXType::Output) &&
        std::find(true_boundary.begin(), true_boundary.end(), *vi) ==
            true_boundary.end()) {
      WireVec adj_wires = zxd.adj_wires(*vi);
      TKET_ASSERT(adj_wires.size() == 2);
      TKET_ASSERT(zxd.get_qtype(adj_wires[0]) == zxd.get_qtype(adj_wires[1]));
      TKET_ASSERT(
          zxd.get_wire_type(adj_wires[0]) == zxd.get_wire_type(adj_wires[1]));
      ZXVertVec neighbours = zxd.neighbours(*vi);
      zxd.add_wire(
          neighbours[0], neighbours[1], zxd.get_wire_type(adj_wires[0]),
          zxd.get_qtype(adj_wires[0]));
      zxd.remove_vertex(*vi);
    }
  }
  return {std::move(zxd), std::move(bmap)};
}

void clean_frontier(
    ZXDiagram& diag, ZXVertVec& frontier, Circuit& circ,
    std::map<ZXVert, unsigned>& qubit_map,
    std::map<ZXVert, ZXVert>& input_qubits) {
  std::set<ZXVert> frontier_lookup;
  std::list<std::pair<ZXVert, ZXVert>> czs;
  for (const ZXVert& f : frontier) {
    frontier_lookup.insert(f);
    for (const Wire& w : diag.adj_wires(f)) {
      ZXVert n = diag.other_end(w, f);
      if (diag.get_zxtype(n) == ZXType::Output) {
        unsigned q = qubit_map.at(n);
        if (diag.get_wire_type(w) == ZXWireType::H) {
          diag.set_wire_type(w, ZXWireType::Basic);
          circ.add_op<unsigned>(OpType::H, {q});
        }
        switch (diag.get_zxtype(f)) {
          case ZXType::Input: {
            break;
          }
          case ZXType::XY: {
            const PhasedGen& f_gen = diag.get_vertex_ZXGen<PhasedGen>(f);
            Expr ph = f_gen.get_param();
            if (!equiv_0(ph)) {
              circ.add_op<unsigned>(OpType::U1, -ph, {q});
            }
            diag.set_vertex_ZXGen_ptr(
                f, ZXGen::create_gen(ZXType::PX, QuantumType::Quantum));
            break;
          }
          case ZXType::PX: {
            const CliffordGen& f_gen = diag.get_vertex_ZXGen<CliffordGen>(f);
            if (f_gen.get_param()) {
              circ.add_op<unsigned>(OpType::Z, {q});
              diag.set_vertex_ZXGen_ptr(
                  f, ZXGen::create_gen(ZXType::PX, QuantumType::Quantum));
            }
            break;
          }
          case ZXType::PY: {
            const CliffordGen& f_gen = diag.get_vertex_ZXGen<CliffordGen>(f);
            circ.add_op<unsigned>(
                f_gen.get_param() ? OpType::S : OpType::Sdg, {q});
            diag.set_vertex_ZXGen_ptr(
                f, ZXGen::create_gen(ZXType::PX, QuantumType::Quantum));
            break;
          }
          default:
            throw ZXError(
                "Error during extraction from ZX diagram: unexpected ZXType in "
                "frontier");
        }
      } else if (frontier_lookup.find(n) != frontier_lookup.end()) {
        czs.push_back({f, n});
        diag.remove_wire(w);
      }
    }
  }
  for (const std::pair<ZXVert, ZXVert>& pair : czs) {
    circ.add_op<unsigned>(
        OpType::CZ, {qubit_map.at(pair.first), qubit_map.at(pair.second)});
  }
  ZXVertVec new_frontier;
  for (const ZXVert& f : frontier) {
    bool removed = false;
    ZXVertVec ns = diag.neighbours(f);
    if (ns.size() == 2) {
      ZXType nt0 = diag.get_zxtype(ns.at(0));
      ZXType nt1 = diag.get_zxtype(ns.at(1));
      if ((nt0 == ZXType::Input && nt1 == ZXType::Output) ||
          (nt0 == ZXType::Output && nt1 == ZXType::Input)) {
        diag.add_wire(ns.at(0), ns.at(1));
        qubit_map.erase(f);
        diag.remove_vertex(f);
        auto input_found = input_qubits.find(f);
        if (input_found != input_qubits.end()) input_qubits.erase(f);
        removed = true;
      }
    }
    if (!removed && diag.get_zxtype(f) != ZXType::Input)
      new_frontier.push_back(f);
  }
  frontier = new_frontier;
}

ZXVertSeqSet neighbours_of_frontier(
    const ZXDiagram& diag, const ZXVertVec& frontier) {
  ZXVertSeqSet n_set;
  for (const ZXVert& f : frontier) {
    for (const Wire& w : diag.adj_wires(f)) {
      ZXVert n = diag.other_end(w, f);
      ZXType n_type = diag.get_zxtype(n);
      if (n_type == ZXType::Output || n_type == ZXType::Input) continue;
      n_set.insert(n);
    }
  }
  return n_set;
}

static void bipartite_complementation(
    ZXDiagram& diag, const ZXVertSeqSet& sa, const ZXVertSeqSet& sb) {
  for (const ZXVert& a : sa.get<TagSeq>()) {
    for (const ZXVert& b : sb.get<TagSeq>()) {
      std::optional<Wire> wire = diag.wire_between(a, b);
      if (wire)
        diag.remove_wire(*wire);
      else
        diag.add_wire(a, b, ZXWireType::H);
    }
  }
}

void extend_if_input(
    ZXDiagram& diag, const ZXVert& v, std::map<ZXVert, ZXVert>& input_qubits) {
  std::map<ZXVert, ZXVert>::iterator found = input_qubits.find(v);
  if (found != input_qubits.end()) {
    ZXVert in = found->second;
    ZXVert ext0 = diag.add_vertex(ZXType::XY);
    ZXVert ext1 = diag.add_vertex(ZXType::XY);
    diag.remove_wire(diag.adj_wires(in).at(0));
    diag.add_wire(in, ext0);
    diag.add_wire(ext0, ext1, ZXWireType::H);
    diag.add_wire(ext1, v, ZXWireType::H);
    input_qubits.erase(found);
    input_qubits.insert({ext0, in});
  }
}

bool remove_all_gadgets(
    ZXDiagram& diag, const ZXVertVec& frontier,
    std::map<ZXVert, ZXVert>& input_qubits) {
  bool removed_gadget = false;
  for (const ZXVert& f : frontier) {
    ZXVertVec f_ns = diag.neighbours(f);
    std::optional<ZXVert> found_output;
    for (const ZXVert& n : f_ns) {
      // Each frontier vertex is connected to a unique output, find it
      if (diag.get_zxtype(n) == ZXType::Output) {
        found_output = n;
        break;
      }
    }
    if (!found_output)
      throw ZXError(
          "Error during extraction from ZXDiagram: frontier vertex not "
          "adjacent to an output");
    ZXVert o = *found_output;
    for (const ZXVert& n : f_ns) {
      if (diag.get_zxtype(n) == ZXType::YZ) {
        // Pivot
        // Identify three subsets of neighbours
        ZXVertSeqSet excl_f;
        // Need to recalculate neighbours rather than use f_ns as we might have
        // extended
        extend_if_input(diag, f, input_qubits);
        for (const ZXVert& n : diag.neighbours(f)) {
          extend_if_input(diag, n, input_qubits);
          excl_f.insert(n);
        }
        auto& lookup_f = excl_f.get<TagKey>();
        lookup_f.erase(lookup_f.find(n));
        lookup_f.erase(lookup_f.find(o));
        ZXVertSeqSet excl_n, joint;
        for (const ZXVert& nn : diag.neighbours(n)) {
          extend_if_input(diag, nn, input_qubits);
          if (lookup_f.find(nn) != lookup_f.end())
            joint.insert(nn);
          else
            excl_n.insert(nn);
        }
        auto& lookup_n = excl_n.get<TagKey>();
        lookup_n.erase(lookup_n.find(f));
        for (const ZXVert& nn : joint.get<TagSeq>()) {
          auto found = lookup_f.find(nn);
          if (found != lookup_f.end()) lookup_f.erase(found);
        }
        // The is_MBQC check in zx_to_circuit guarantees QuantumType::Quantum
        bipartite_complementation(diag, joint, excl_n);
        bipartite_complementation(diag, joint, excl_f);
        bipartite_complementation(diag, excl_n, excl_f);
        // In place of switching vertices f and n, we invert their
        // connectivities
        for (const ZXVert& v : excl_f.get<TagSeq>()) excl_n.insert(v);
        bipartite_complementation(diag, {f}, excl_n);
        bipartite_complementation(diag, {n}, excl_n);
        Wire ow = *diag.wire_between(f, o);
        diag.set_wire_type(
            ow, (diag.get_wire_type(ow) == ZXWireType::Basic)
                    ? ZXWireType::H
                    : ZXWireType::Basic);
        const PhasedGen& yz_gen = diag.get_vertex_ZXGen<PhasedGen>(n);
        diag.set_vertex_ZXGen_ptr(
            n, ZXGen::create_gen(
                   ZXType::XY, -yz_gen.get_param(), QuantumType::Quantum));
        for (const ZXVert& nn : joint.get<TagSeq>()) {
          ZXGen_ptr new_gen;
          switch (diag.get_zxtype(nn)) {
            case ZXType::XY: {
              const PhasedGen& ph_gen = diag.get_vertex_ZXGen<PhasedGen>(nn);
              new_gen = ZXGen::create_gen(
                  ZXType::XY, ph_gen.get_param() + 1., QuantumType::Quantum);
              break;
            }
            case ZXType::PX:
            case ZXType::PY: {
              const CliffordGen& cl_gen =
                  diag.get_vertex_ZXGen<CliffordGen>(nn);
              new_gen = ZXGen::create_gen(
                  cl_gen.get_type(), !cl_gen.get_param(), QuantumType::Quantum);
              break;
            }
            case ZXType::XZ:
            case ZXType::YZ: {
              const PhasedGen& ph_gen = diag.get_vertex_ZXGen<PhasedGen>(nn);
              new_gen = ZXGen::create_gen(
                  ph_gen.get_type(), -ph_gen.get_param(), QuantumType::Quantum);
              break;
            }
            case ZXType::PZ: {
              new_gen = diag.get_vertex_ZXGen_ptr(nn);
              break;
            }
            default:
              throw ZXError(
                  "Error during extraction from ZX diagram: unexpected ZXType "
                  "during local complementation");
          }
          diag.set_vertex_ZXGen_ptr(nn, new_gen);
        }
        removed_gadget = true;
        break;
      }
    }
  }
  return removed_gadget;
}

Circuit zx_to_circuit(const ZXDiagram& d) {
  ZXDiagram diag = d;
  if (!diag.is_MBQC())
    throw ZXError("Can only extract a circuit from a ZX diagram in MBQC form");
  ZXVertVec ins = diag.get_boundary(ZXType::Input);
  ZXVertVec outs = diag.get_boundary(ZXType::Output);
  if (ins.size() != outs.size())
    throw ZXError("Can only extract a circuit from a unitary ZX diagram");

  BGL_FORALL_VERTICES(v, *diag.get_graph(), ZXGraph) {
    ZXGen_ptr vgen = diag.get_vertex_ZXGen_ptr(v);
    if (vgen->get_type() == ZXType::PY) {
      const CliffordGen& cgen = dynamic_cast<const CliffordGen&>(*vgen);
      diag.set_vertex_ZXGen_ptr(
          v, ZXGen::create_gen(
                 ZXType::XY, cgen.get_param() ? Expr(-0.5) : Expr(0.5),
                 *cgen.get_qtype()));
    }
  }

  Circuit circ(ins.size());

  ZXVertVec frontier;
  std::map<ZXVert, unsigned> qubit_map;
  unsigned q = 0;
  for (const ZXVert& o : outs) {
    ZXVert f_i = diag.neighbours(o).at(0);
    frontier.push_back(f_i);
    qubit_map.insert({o, q});
    qubit_map.insert({f_i, q});
    ++q;
  }
  std::map<ZXVert, ZXVert> input_qubits;
  for (const ZXVert& i : ins)
    input_qubits.insert({diag.neighbours(i).at(0), i});

  clean_frontier(diag, frontier, circ, qubit_map, input_qubits);
  while (!frontier.empty()) {
    if (remove_all_gadgets(diag, frontier, input_qubits)) {
      clean_frontier(diag, frontier, circ, qubit_map, input_qubits);
    }
    ZXVertSeqSet neighbours = neighbours_of_frontier(diag, frontier);
    boost::bimap<ZXVert, unsigned> correctors, preserve, ys;
    ZXVertVec to_solve;
    for (const ZXVert& f : frontier) {
      if (input_qubits.find(f) == input_qubits.end())
        correctors.insert({f, (unsigned)correctors.size()});
    }
    for (const ZXVert& n : neighbours.get<TagSeq>()) {
      ZXType n_type = diag.get_zxtype(n);
      if (n_type == ZXType::XY || n_type == ZXType::PX) {
        preserve.insert({n, (unsigned)preserve.size()});
        to_solve.push_back(n);
      }
    }
    std::map<ZXVert, ZXVertSeqSet> candidates =
        Flow::gauss_solve_correctors(diag, correctors, preserve, to_solve, ys);

    if (candidates.empty())
      throw ZXError(
          "Error during extraction from ZX diagram: diagram does not have "
          "gflow");

    unsigned min = UINT_MAX;
    ZXVert best;
    for (const ZXVert& v : to_solve) {
      auto found = candidates.find(v);
      if (found != candidates.end() && found->second.size() < min) {
        min = found->second.size();
        best = v;
      }
    }
    ZXVertSeqSet g_best = candidates.at(best);

    ZXVert f_to_isolate = g_best.get<TagSeq>().front();
    unsigned f_q = qubit_map.at(f_to_isolate);
    for (const ZXVert& f : g_best.get<TagSeq>()) {
      if (f != f_to_isolate) {
        circ.add_op<unsigned>(OpType::CX, {f_q, qubit_map.at(f)});
      }
    }
    ZXVert out;
    Wire w_out;
    for (const Wire& w : diag.adj_wires(f_to_isolate)) {
      ZXVert n = diag.other_end(w, f_to_isolate);
      if (diag.get_zxtype(n) == ZXType::Output) {
        out = n;
        w_out = w;
        break;
      }
    }
    diag.add_wire(
        best, out,
        (diag.get_wire_type(w_out) == ZXWireType::Basic) ? ZXWireType::H
                                                         : ZXWireType::Basic);
    diag.remove_vertex(f_to_isolate);
    qubit_map.erase(qubit_map.find(f_to_isolate));
    qubit_map.insert({best, f_q});
    for (ZXVertVec::iterator it = frontier.begin(); it != frontier.end();
         ++it) {
      if (*it == f_to_isolate) {
        *it = best;
        break;
      }
    }

    clean_frontier(diag, frontier, circ, qubit_map, input_qubits);
  }

  qubit_map_t qm;
  for (unsigned i = 0; i < ins.size(); ++i) {
    ZXVert in = ins.at(i);
    ZXVert out = diag.neighbours(in).at(0);
    if (diag.get_zxtype(out) != ZXType::Output)
      throw ZXError(
          "Error during extraction from ZX diagram: input not adjacent to "
          "output after extracting");
    qm.insert({Qubit(qubit_map.at(out)), Qubit(i)});
  }
  circ.permute_boundary_output(qm);

  // Reverse gates in circuit (all gates added are self-transpose)
  return circ.transpose();
}

}  // namespace tket
