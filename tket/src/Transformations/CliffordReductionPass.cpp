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

#include "tket/Transformations/CliffordReductionPass.hpp"

#include "tket/Circuit/CircUtils.hpp"
#include "tket/Circuit/DAGDefs.hpp"
#include "tket/PauliGraph/ConjugatePauliFunctions.hpp"

namespace tket {

/**
 * Finds Clifford circuit C such that
 * R[p](a); R[q](b) = C; RZ(a); RZ(b); C^\dagger if p==q or
 *                    C; RZ(a); RY(b); C^\dagger if p!=q
 */
static const std::map<std::pair<Pauli, Pauli>, std::list<OpType>>
    mapping_to_zz_or_zy_lut{
        {{Pauli::X, Pauli::X}, {OpType::H}},
        {{Pauli::X, Pauli::Y}, {OpType::H, OpType::Z}},
        {{Pauli::X, Pauli::Z}, {OpType::H, OpType::S}},
        {{Pauli::Y, Pauli::X}, {OpType::V, OpType::S}},
        {{Pauli::Y, Pauli::Y}, {OpType::V}},
        {{Pauli::Y, Pauli::Z}, {OpType::V, OpType::Z}},
        {{Pauli::Z, Pauli::X}, {OpType::S}},
        {{Pauli::Z, Pauli::Y}, {}},
        {{Pauli::Z, Pauli::Z}, {}},
    };

static const std::map<Pauli, OpType> pauli_to_pauli_gate_lut{
    {Pauli::X, OpType::X},
    {Pauli::Y, OpType::Y},
    {Pauli::Z, OpType::Z},
};

/**
 * Consider an interaction of R[p0, p1](+-0.5); R[q0, q1](+-0.5)
 * where p0, p1, q0, q1 in {X, Y, Z}.
 * Returns the equivalent replacement circuit with fewer 2qb interactions.
 */
static Circuit interaction_replacement(const InteractionMatch &match) {
  const Pauli &p0 = match.point0.p;
  const Pauli &p1 = match.point1.p;
  const Pauli &q0 = match.rev0.p;
  const Pauli &q1 = match.rev1.p;
  Circuit replacement(2);
  if (match.point0.phase ^ match.point1.phase) {
    replacement.add_op<unsigned>(pauli_to_pauli_gate_lut.at(p0), {0});
    replacement.add_op<unsigned>(pauli_to_pauli_gate_lut.at(p1), {1});
    replacement.add_phase(0.5);
  }
  if (p0 == q0) {
    if (p1 == q1) {
      // R[p0, p1](1) = R[p0, I](1); R[I, p1](1)
      OpType op0 = pauli_to_pauli_gate_lut.at(p0);
      OpType op1 = pauli_to_pauli_gate_lut.at(p1);
      replacement.add_op<unsigned>(op0, {0});
      replacement.add_op<unsigned>(op1, {1});
      replacement.add_phase(-0.5);
    } else {
      // Map to R[Z, Z](0.5); R[Z, Y](0.5)
      Circuit basis_change(2);
      std::list<OpType> ops0 = mapping_to_zz_or_zy_lut.at({p0, q0});
      std::list<OpType> ops1 = mapping_to_zz_or_zy_lut.at({p1, q1});
      for (OpType op : ops0) {
        basis_change.add_op<unsigned>(op, {0});
      }
      for (OpType op : ops1) {
        basis_change.add_op<unsigned>(op, {1});
      }
      replacement.append(basis_change);
      replacement.add_op<unsigned>(OpType::V, {1});
      replacement.add_op<unsigned>(OpType::ZZMax, {0, 1});
      replacement.append(basis_change.dagger());
    }
  } else {
    if (p1 == q1) {
      // Map to R[Z, Z](0.5); R[Y, Z](0.5)
      Circuit basis_change(2);
      std::list<OpType> ops0 = mapping_to_zz_or_zy_lut.at({p0, q0});
      std::list<OpType> ops1 = mapping_to_zz_or_zy_lut.at({p1, q1});
      for (OpType op : ops0) {
        basis_change.add_op<unsigned>(op, {0});
      }
      for (OpType op : ops1) {
        basis_change.add_op<unsigned>(op, {1});
      }
      replacement.append(basis_change);
      replacement.add_op<unsigned>(OpType::V, {0});
      replacement.add_op<unsigned>(OpType::ZZMax, {0, 1});
      replacement.append(basis_change.dagger());
    } else {
      // Map to R[Z, Z](0.5); R[Y, Y](0.5)
      Circuit basis_change(2);
      std::list<OpType> ops0 = mapping_to_zz_or_zy_lut.at({p0, q0});
      std::list<OpType> ops1 = mapping_to_zz_or_zy_lut.at({p1, q1});
      for (OpType op : ops0) {
        basis_change.add_op<unsigned>(op, {0});
      }
      for (OpType op : ops1) {
        basis_change.add_op<unsigned>(op, {1});
      }
      replacement.append(basis_change);
      replacement.add_op<unsigned>(OpType::H, {0});
      replacement.add_op<unsigned>(OpType::H, {1});
      replacement.add_op<unsigned>(OpType::Z, {0});
      replacement.add_op<unsigned>(OpType::Z, {1});
      replacement.add_op<unsigned>(OpType::ZZMax, {0, 1});
      replacement.add_op<unsigned>(OpType::H, {0});
      replacement.add_op<unsigned>(OpType::H, {1});
      replacement.add_op<unsigned>(OpType::SWAP, {0, 1});
      replacement.add_phase(0.25);
      replacement.append(basis_change.dagger());
    }
  }
  if (match.rev0.phase ^ match.rev1.phase) {
    replacement.add_op<unsigned>(pauli_to_pauli_gate_lut.at(q0), {0});
    replacement.add_op<unsigned>(pauli_to_pauli_gate_lut.at(q1), {1});
    replacement.add_phase(0.5);
  }
  return replacement;
}

/**
 * Given a 2qb Clifford gate, returns just the local operations applied around
 * the maximally-entangling gadget.
 */
static Circuit local_cliffords(OpType op) {
  Circuit locals(2);
  switch (op) {
    case OpType::CX: {
      locals.add_op<unsigned>(OpType::Sdg, {0});
      locals.add_op<unsigned>(OpType::Vdg, {1});
      break;
    }
    case OpType::CZ: {
      locals.add_op<unsigned>(OpType::Sdg, {0});
      locals.add_op<unsigned>(OpType::Sdg, {1});
      locals.add_phase(0.25);
      break;
    }
    case OpType::CY: {
      locals.add_op<unsigned>(OpType::Sdg, {0});
      locals.add_op<unsigned>(OpType::V, {1});
      locals.add_op<unsigned>(OpType::Sdg, {1});
      locals.add_op<unsigned>(OpType::Vdg, {1});
      locals.add_phase(0.25);
      break;
    }
    case OpType::ZZMax: {
      break;
    }
    default: {
      throw CircuitInvalidity(
          "Attempting to replace non-Clifford gate with Clifford "
          "optimisation");
      break;
    }
  }
  return locals;
}

void CliffordReductionPass::insert_interaction_point(InteractionPoint ip) {
  itable.insert(ip);
  Vertex next = circ.target(ip.e);
  port_t next_p = circ.get_target_port(ip.e);
  bool commute = true;
  while (commute) {
    if (v_to_depth.find(next) == v_to_depth.end()) {
      commute = false;
      continue;
    }
    Op_ptr op = circ.get_Op_ptr_from_Vertex(next);
    if (!op->get_desc().is_gate()) {
      commute = false;
      continue;
    }
    OpType type = op->get_type();
    switch (type) {
      case OpType::H:
      case OpType::S:
      case OpType::Sdg:
      case OpType::V:
      case OpType::Vdg:
      case OpType::X:
      case OpType::Y:
      case OpType::Z: {
        std::pair<Pauli, bool> new_basis = conjugate_Pauli(type, ip.p, true);
        ip.p = new_basis.first;
        ip.phase ^= new_basis.second;
        break;
      }
      case OpType::SWAP: {
        next_p = 1 - next_p;
        break;
      }
      default: {
        if (!circ.commutes_with_basis(next, ip.p, next_p)) {
          commute = false;
          continue;
        }
        break;
      }
    }
    ip.e = circ.get_nth_out_edge(next, next_p);
    auto inserted = itable.insert(ip);
    commute = inserted.second;
    if (!commute) {
      // Now `inserted.first` points to the element of the table that
      // blocked insertion, i.e. had the same source/edge combination.
      // Check that its `p` and `phase` are correct: if not, something has
      // gone wrong.
      auto blocker = inserted.first;
      TKET_ASSERT(blocker->p == ip.p && blocker->phase == ip.phase);
    }
    next = circ.target(ip.e);
    next_p = circ.get_target_port(ip.e);
  }
}

std::optional<InteractionMatch> CliffordReductionPass::search_back_for_match(
    const RevInteractionPoint &rip0, const RevInteractionPoint &rip1) const {
  RevInteractionPoint point[2];
  point[0] = rip0;
  point[1] = rip1;
  std::map<Edge, RevInteractionPoint> point_lookup;

  // interactions met when commuting back; point lists are in causal order of
  // circuit:
  std::map<IVertex, std::list<InteractionPoint>> candidates[2];

  for (unsigned i = 0; i < 2; ++i) {
    // Commute edge i back as far as possible
    bool commute = true;
    while (commute) {
      point_lookup.insert({point[i].e, point[i]});
      auto r = itable.get<TagEdge>().equal_range(point[i].e);
      for (auto it = r.first; it != r.second; ++it) {
        Vertex v = it->source;
        candidates[i][{im.at(v), v}].push_front(*it);
      }
      Vertex pred = circ.source(point[i].e);
      port_t pred_port = circ.get_source_port(point[i].e);
      Op_ptr pred_op = circ.get_Op_ptr_from_Vertex(pred);
      if (!pred_op->get_desc().is_gate()) {
        commute = false;
        continue;
      }
      OpType type = pred_op->get_type();
      switch (type) {
        case OpType::H:
        case OpType::S:
        case OpType::Sdg:
        case OpType::V:
        case OpType::Vdg:
        case OpType::X:
        case OpType::Y:
        case OpType::Z: {
          std::pair<Pauli, bool> new_basis = conjugate_Pauli(type, point[i].p);
          point[i].p = new_basis.first;
          point[i].phase ^= new_basis.second;
          break;
        }
        case OpType::SWAP: {
          pred_port = 1 - pred_port;
          break;
        }
        default: {
          commute = circ.commutes_with_basis(pred, point[i].p, pred_port);
          break;
        }
      }
      point[i].e = circ.get_nth_in_edge(pred, pred_port);
    }
  }
  // Check for matching interactions
  for (const std::pair<const IVertex, std::list<InteractionPoint>> &pair :
       candidates[0]) {
    auto found = candidates[1].find(pair.first);
    if (found != candidates[1].end()) {
      std::optional<std::pair<InteractionPoint, InteractionPoint>>
          insert_point = valid_insertion_point(pair.second, found->second);
      if (insert_point) {
        InteractionMatch match = {
            insert_point->first, insert_point->second,
            point_lookup.at(insert_point->first.e),
            point_lookup.at(insert_point->second.e)};
        if (!allow_swaps) {
          if (match.point0.p != match.rev0.p && match.point1.p != match.rev1.p)
            continue;
        }
        return match;
      }
    }
  }
  return std::nullopt;
}

void CliffordReductionPass::process_new_interaction(const Vertex &inter) {
  // Process the vertex as well as any new 2qb Cliffords that get inserted
  // while doing so (and so on until there are none left to process).
  std::list<Vertex> to_process = {inter};
  while (!to_process.empty()) {
    Vertex v = to_process.front();
    to_process.pop_front();
    Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
    Pauli basis0 = *op->commuting_basis(0);
    Pauli basis1 = *op->commuting_basis(1);
    EdgeVec ins = circ.get_in_edges(v);
    RevInteractionPoint rip0 = {ins.at(0), basis0, false};
    RevInteractionPoint rip1 = {ins.at(1), basis1, false};
    std::optional<InteractionMatch> match = search_back_for_match(rip0, rip1);
    if (match) {
      Circuit replacement = interaction_replacement(*match);
      Subcircuit site;
      site.in_hole = {match->point0.e, match->point1.e};
      site.out_hole = {match->point0.e, match->point1.e};
      Subcircuit inserted = substitute(replacement, site);
      const Vertex &source = match->point0.source;
      Circuit source_locals =
          local_cliffords(circ.get_OpType_from_Vertex(source));
      Subcircuit source_site = circ.singleton_subcircuit(source);
      substitute(source_locals, source_site);
      Circuit v_locals = local_cliffords(op->get_type());
      Subcircuit v_site = circ.singleton_subcircuit(v);
      substitute(v_locals, v_site);
      for (const Vertex &new_v : inserted.verts) {
        if (circ.n_in_edges(new_v) == 2 &&
            circ.get_OpType_from_Vertex(new_v) != OpType::SWAP) {
          to_process.push_back(new_v);
          break;
        }
      }
      im = circ.index_map();
      success = true;
    } else {
      std::vector<std::optional<Edge>> outs = circ.get_linear_out_edges(v);
      InteractionPoint ip0 = {*outs.at(0), v, basis0, false};
      insert_interaction_point(ip0);
      InteractionPoint ip1 = {*outs.at(1), v, basis1, false};
      insert_interaction_point(ip1);
    }
  }
}

Subcircuit CliffordReductionPass::substitute(
    const Circuit &to_insert, const Subcircuit &to_replace) {
  unsigned q_width = to_replace.in_hole.size();
  TKET_ASSERT(q_width == 2);

  // Construct tables of predecessors, successors, units, in-edges, out-edges.
  // Only quantum circuit replacements here so don't care about classical stuff
  std::vector<VertPort> preds(q_width);
  std::vector<VertPort> succs(q_width);
  std::vector<UnitID> units(q_width);
  std::vector<Edge> in_edges(q_width);
  std::vector<Edge> out_edges(q_width);
  for (unsigned qi = 0; qi < q_width; ++qi) {
    const Edge &in = to_replace.in_hole.at(qi);
    const Edge &out = *to_replace.out_hole.at(qi);
    preds[qi] = {circ.source(in), circ.get_source_port(in)};
    succs[qi] = {circ.target(out), circ.get_target_port(out)};
    units[qi] = e_to_unit.at(in);
    in_edges[qi] = in;
    out_edges[qi] = out;
    TKET_ASSERT(in == out || circ.target(in) == circ.source(out));
  }

  // List of points that will be invalidated by the substitution.
  std::list<InteractionPoint> invalidated_points;

  // Lists of points having the same "in"/"out" edge as the replacement:
  // These are all invalidated (though the "in" ones can be replaced later
  // with the new edge).
  std::vector<std::list<InteractionPoint>> points_with_in(q_width);
  std::vector<std::list<InteractionPoint>> points_with_out(q_width);
  for (unsigned qi = 0; qi < q_width; ++qi) {
    auto r = itable.get<TagEdge>().equal_range(in_edges[qi]);
    for (auto it = r.first; it != r.second; it++) {
      points_with_in[qi].push_back(*it);
      invalidated_points.push_back(*it);
    }
    r = itable.get<TagEdge>().equal_range(out_edges[qi]);
    for (auto it = r.first; it != r.second; it++) {
      points_with_out[qi].push_back(*it);
      invalidated_points.push_back(*it);
    }
  }

  // For any (e0, v0) in points_with_out, any point (e1, v0) where e1 is in
  // the causal future of e0 is also invalidated. Calculate all the future
  // edges.
  EdgeList future_edges;
  VertexSet v_frontier;
  // keep track of visited vertices
  VertexSet visited;
  for (unsigned qi = 0; qi < q_width; ++qi) {
    Vertex target = circ.target(out_edges[qi]);
    visited.insert(target);
    v_frontier.insert(target);
  }
  while (!v_frontier.empty()) {
    EdgeSet out_edges;
    for (auto v : v_frontier) {
      if (v_to_depth.find(v) != v_to_depth.end()) {
        EdgeVec v_out_edges = circ.get_out_edges_of_type(v, EdgeType::Quantum);
        out_edges.insert(v_out_edges.begin(), v_out_edges.end());
      }
    }
    future_edges.insert(future_edges.end(), out_edges.begin(), out_edges.end());
    VertexSet new_v_frontier;
    for (auto e : out_edges) {
      Vertex target = circ.target(e);
      auto it = visited.insert(target);
      if (it.second) {
        // add vertex to the next frontier if the vertex has not been visited
        new_v_frontier.insert(target);
      }
    }
    v_frontier = std::move(new_v_frontier);
  }

  // Invalidate the (e1, v0) as above.
  VertexSet invalid_sources;
  for (unsigned qi = 0; qi < q_width; ++qi) {
    for (auto ip : points_with_out[qi]) {
      TKET_ASSERT(ip.e == out_edges[qi]);
      invalid_sources.insert(ip.source);
    }
  }
  for (auto e1 : future_edges) {
    auto r = itable.get<TagEdge>().equal_range(e1);
    for (auto it = r.first; it != r.second; ++it) {
      if (invalid_sources.find(it->source) != invalid_sources.end()) {
        invalidated_points.push_back(*it);
      }
    }
  }

  // Erase the invalidated points from the table.
  for (auto ip : invalidated_points) {
    itable.erase(ip.key());
  }

  // Erase edges from e_to_unit
  for (unsigned qi = 0; qi < q_width; ++qi) {
    e_to_unit.erase(in_edges[qi]);
    e_to_unit.erase(out_edges[qi]);
    // Depth of to_replace is at most 1, so there are no other edges
  }

  // Remove replaced vertices from depth and units maps and erase all points
  // from the itable that have a replaced vertex as source.
  for (const Vertex &v : to_replace.verts) {
    v_to_depth.erase(v);
    v_to_units.erase(v);
    auto r = itable.get<TagSource>().equal_range(v);
    for (auto next = r.first; next != r.second; r.first = next) {
      ++next;
      itable.erase(itable.project<TagKey>(r.first));
    }
  }

  circ.substitute(to_insert, to_replace);

  // Update the tables of in and out edges, and amend the stored points
  for (unsigned qi = 0; qi < q_width; ++qi) {
    in_edges[qi] = circ.get_nth_out_edge(preds[qi].first, preds[qi].second);
    out_edges[qi] = circ.get_nth_in_edge(succs[qi].first, succs[qi].second);
    for (InteractionPoint &ip : points_with_in[qi]) {
      ip.e = in_edges[qi];
    }
  }

  // Construct inserted Subcircuit, update e_to_unit and v_to_units, and
  // add new vertices to v_to_depth, with (temporary) value 0.
  Subcircuit inserted;
  for (unsigned qi = 0; qi < q_width; ++qi) {
    Edge in = in_edges[qi];
    inserted.in_hole.push_back(in);
    Edge out = out_edges[qi];
    inserted.out_hole.push_back(out);
    while (in != out) {
      Vertex next = circ.target(in);
      inserted.verts.insert(next);
      e_to_unit.insert({in, units[qi]});
      v_to_depth.insert({next, 0});
      v_to_units[next].insert(units[qi]);
      in = circ.get_nth_out_edge(next, circ.get_target_port(in));
    }
    e_to_unit.insert({in, units[qi]});
  }

  // Now `v_to_depth` is 0 at all `inserted.verts`. Fix this and propagate
  // updates to the depth map into the future cone, ensuring that the depths
  // are strictly increasing along wires. Stop when we reach a vertex that
  // isn't already in v_to_depth (because we haven't processed it yet).
  for (unsigned qi = 0; qi < q_width; ++qi) {
    Edge in = in_edges[qi];
    Vertex next = circ.target(in);
    if (next != succs[qi].first) {
      if (v_to_depth.at(preds[qi].first) >= v_to_depth[next]) {
        // We may have already set v_to_depth[next] to a higher value
        // when tracing another qubit, so the check above is necessary.
        v_to_depth[next] = v_to_depth.at(preds[qi].first) + 1;
      }
      std::function<bool(const Vertex &, const Vertex &)> c =
          [&](const Vertex &a, const Vertex &b) {
            unsigned deptha = v_to_depth.at(a);
            unsigned depthb = v_to_depth.at(b);
            if (deptha == depthb) {
              unit_set_t unitsa = v_to_units.at(a);
              unit_set_t unitsb = v_to_units.at(b);
              return unitsa < unitsb;
            }
            return deptha < depthb;
          };
      std::set<Vertex> to_search;
      to_search.insert(next);
      while (!to_search.empty()) {
        Vertex v = *std::min_element(to_search.begin(), to_search.end(), c);
        to_search.erase(v);
        unsigned v_depth = v_to_depth.at(v);
        EdgeVec outs = circ.get_all_out_edges(v);
        for (const Edge &e : outs) {
          Vertex succ = circ.target(e);
          std::map<Vertex, unsigned>::iterator succ_it = v_to_depth.find(succ);
          if (succ_it != v_to_depth.end()) {
            if (succ_it->second <= v_depth) {
              succ_it->second = v_depth + 1;
              if (v_depth >= current_depth) {
                current_depth = v_depth + 1;
              }
              to_search.insert(succ);
            }
          }
        }
      }
    }
  }

  // Re-insert all the interaction points we erased above.
  for (unsigned qi = 0; qi < q_width; ++qi) {
    for (const InteractionPoint &ip : points_with_in[qi]) {
      insert_interaction_point(ip);
    }
  }

  return inserted;
}

std::optional<Edge> CliffordReductionPass::find_earliest_successor(
    const Edge &source, const EdgeSet &candidates) const {
  typedef std::function<bool(Vertex, Vertex)> Comp;
  Comp c = [&](Vertex a, Vertex b) {
    unsigned deptha = v_to_depth.at(a);
    unsigned depthb = v_to_depth.at(b);
    if (deptha == depthb) {
      unit_set_t unitsa = v_to_units.at(a);
      unit_set_t unitsb = v_to_units.at(b);
      return unitsa < unitsb;
    }
    return deptha < depthb;
  };
  std::set<Vertex, Comp> to_search(c);
  to_search.insert(circ.target(source));
  while (!to_search.empty()) {
    Vertex v = *to_search.begin();
    to_search.erase(to_search.begin());
    EdgeVec outs = circ.get_all_out_edges(v);
    for (const Edge &e : outs) {
      if (candidates.find(e) != candidates.end()) return e;
      Vertex succ = circ.target(e);
      if (v_to_depth.find(succ) != v_to_depth.end()) to_search.insert(succ);
    }
  }
  return std::nullopt;
}

std::optional<std::pair<InteractionPoint, InteractionPoint>>
CliffordReductionPass::valid_insertion_point(
    const std::list<InteractionPoint> &seq0,
    const std::list<InteractionPoint> &seq1) const {
  // seq0 is chain of edges (in temporal order) from the first qubit
  // likewise seq1 for the other qubit
  InteractionPoint seq0max = seq0.back();
  InteractionPoint seq1max = seq1.back();
  if (circ.in_causal_order(
          circ.source(seq1max.e), circ.target(seq0max.e), true, v_to_depth,
          v_to_units, false)) {
    // Search for any points in seq1 from future of seq0max
    EdgeSet candidates;
    std::map<Edge, InteractionPoint> lookup;
    for (const InteractionPoint &ip : seq1) {
      candidates.insert(ip.e);
      lookup.insert({ip.e, ip});
    }
    std::optional<Edge> successor =
        find_earliest_successor(seq0max.e, candidates);
    if (!successor || successor == seq1.front().e) return std::nullopt;
    Vertex v = circ.source(*successor);
    port_t p = circ.get_source_port(*successor);
    if (circ.get_OpType_from_Vertex(v) == OpType::SWAP) p = 1 - p;
    return {{seq0max, lookup.at(circ.get_nth_in_edge(v, p))}};
  } else if (
      circ.in_causal_order(
          circ.source(seq0max.e), circ.target(seq1max.e), true, v_to_depth,
          v_to_units, false)) {
    // Search for any points in seq0 from future of seq1max
    EdgeSet candidates;
    std::map<Edge, InteractionPoint> lookup;
    for (const InteractionPoint &ip : seq0) {
      candidates.insert(ip.e);
      lookup.insert({ip.e, ip});
    }
    std::optional<Edge> successor =
        find_earliest_successor(seq1max.e, candidates);
    if (!successor || successor == seq0.front().e) return std::nullopt;
    Vertex v = circ.source(*successor);
    port_t p = circ.get_source_port(*successor);
    if (circ.get_OpType_from_Vertex(v) == OpType::SWAP) p = 1 - p;
    return {{lookup.at(circ.get_nth_in_edge(v, p)), seq1max}};
  } else {
    // seq0max and seq1max are space-like separated
    return {{seq0max, seq1max}};
  }
}

CliffordReductionPass::CliffordReductionPass(Circuit &c, bool swaps)
    : circ(c),
      im(c.index_map()),
      itable(),
      v_to_depth(),
      success(false),
      current_depth(1),
      allow_swaps(swaps) {
  v_to_units = circ.vertex_unit_map();
  e_to_unit = circ.edge_unit_map();
}

bool CliffordReductionPass::reduce_circuit(Circuit &circ, bool allow_swaps) {
  CliffordReductionPass context(circ, allow_swaps);

  SliceVec slices = circ.get_slices();

  for (const Vertex &in : circ.all_inputs()) {
    context.v_to_depth.insert({in, 0});
  }

  // Process all 2qb Clifford vertices.
  for (const Slice &sl : slices) {
    for (const Vertex &v : sl) {
      context.v_to_depth.insert({v, context.current_depth});
      Op_ptr op = circ.get_Op_ptr_from_Vertex(v);
      if (!op->get_desc().is_gate()) continue;
      EdgeVec ins = circ.get_in_edges(v);
      std::vector<std::optional<Edge>> outs = circ.get_linear_out_edges(v);
      OpType type = op->get_type();
      std::list<InteractionPoint> new_points;
      switch (type) {
        case OpType::H:
        case OpType::S:
        case OpType::Sdg:
        case OpType::V:
        case OpType::Vdg:
        case OpType::X:
        case OpType::Y:
        case OpType::Z: {
          auto r = context.itable.get<TagEdge>().equal_range(ins[0]);
          for (auto it = r.first; it != r.second; ++it) {
            InteractionPoint ip = *it;
            std::pair<Pauli, bool> new_basis =
                conjugate_Pauli(type, ip.p, true);
            ip.p = new_basis.first;
            ip.phase ^= new_basis.second;
            ip.e = *outs[0];
            new_points.push_back(ip);
          }
          break;
        }
        case OpType::SWAP:
        case OpType::TK2: {
          if (type == OpType::TK2) {
            std::vector<Expr> params = op->get_params();
            std::optional<double> phase =
                is_TK2_SWAP(params[0], params[1], params[2]);
            if (phase == std::nullopt) {
              goto default_case;
            }
          }
          auto r0 = context.itable.get<TagEdge>().equal_range(ins[0]);
          for (auto it = r0.first; it != r0.second; ++it) {
            InteractionPoint ip = *it;
            ip.e = *outs[1];
            new_points.push_back(ip);
          }
          auto r1 = context.itable.get<TagEdge>().equal_range(ins[1]);
          for (auto it = r1.first; it != r1.second; ++it) {
            InteractionPoint ip = *it;
            ip.e = *outs[0];
            new_points.push_back(ip);
          }
          break;
        }
        default_case:
        default: {
          for (unsigned i = 0; i < ins.size(); ++i) {
            auto r = context.itable.get<TagEdge>().equal_range(ins[i]);
            for (auto it = r.first; it != r.second; ++it) {
              InteractionPoint ip = *it;
              if (circ.commutes_with_basis(v, ip.p, i)) {
                ip.e = *outs[i];
                new_points.push_back(ip);
              }
            }
          }
          break;
        }
      }
      for (const InteractionPoint &ip : new_points) {
        context.itable.insert(ip);
      }
      switch (type) {
        case OpType::CX:
        case OpType::CY:
        case OpType::CZ:
        case OpType::ZZMax: {
          context.process_new_interaction(v);
          break;
        }
        default: {
          break;
        }
      }
    }
    ++context.current_depth;
  }

  if (allow_swaps) {
    context.success = circ.replace_SWAPs(true) || context.success;
  }

  return context.success;
}

namespace Transforms {

Transform clifford_reduction(bool allow_swaps) {
  return Transform([=](Circuit &circ) {
    return CliffordReductionPass::reduce_circuit(circ, allow_swaps);
  });
}

}  // namespace Transforms

CliffordReductionPassTester::CliffordReductionPassTester(Circuit &circ)
    : context(circ, true) {
  // populate v_to_depth
  for (const Vertex &in : circ.all_inputs()) {
    context.v_to_depth.insert({in, 0});
  }

  SliceVec slices = circ.get_slices();
  for (const Slice &sl : slices) {
    for (const Vertex &v : sl) {
      context.v_to_depth.insert({v, context.current_depth});
    }
  }
}

std::optional<std::pair<InteractionPoint, InteractionPoint>>
CliffordReductionPassTester::valid_insertion_point(
    const std::list<InteractionPoint> &seq0,
    const std::list<InteractionPoint> &seq1) const {
  return context.valid_insertion_point(seq0, seq1);
}

}  // namespace tket
