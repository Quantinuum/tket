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

#include "tket/Circuit/Slices.hpp"

#include <algorithm>

#include "tket/Circuit/Circuit.hpp"

namespace tket {

/*SliceIterator related methods*/
SliceIterator::SliceIterator(const Circuit& circ) : cut_(), circ_(&circ) {
  cut_.init();
  // add qubits to u_frontier
  for (const Qubit& q : circ.all_qubits()) {
    Vertex in = circ.get_in(q);
    cut_.slice->push_back(in);
    cut_.u_frontier->insert({q, circ.get_nth_out_edge(in, 0)});
  }

  // add bits to u_frontier and b_frontier
  for (const Bit& b : circ.all_bits()) {
    Vertex in = circ.get_in(b);
    cut_.slice->push_back(in);
    cut_.b_frontier->insert({b, circ.get_nth_b_out_bundle(in, 0)});
    cut_.u_frontier->insert({b, circ.get_nth_out_edge(in, 0)});
  }

  // add WasmState to u_frontier
  for (unsigned i = 0; i < circ._number_of_wasm_wires; ++i) {
    Vertex in = circ.get_in(circ.wasmwire[i]);
    cut_.slice->push_back(in);
    cut_.u_frontier->insert({circ.wasmwire[i], circ.get_nth_out_edge(in, 0)});
  }

  // add RNGState to u_frontier
  for (unsigned i = 0; i < circ._number_of_rng_wires; ++i) {
    Vertex in = circ.get_in(circ.rngwire[i]);
    cut_.slice->push_back(in);
    cut_.u_frontier->insert({circ.rngwire[i], circ.get_nth_out_edge(in, 0)});
  }

  prev_b_frontier_ = cut_.b_frontier;
  cut_ = circ.next_cut(cut_.u_frontier, cut_.b_frontier);

  // Add all vertices that have no Quantum or Classical edges (e.g. Phase) and
  // no Boolean inputs:
  BGL_FORALL_VERTICES(v, circ.dag, DAG) {
    if (circ.n_in_edges(v) == 0 &&
        circ.n_out_edges_of_type(v, EdgeType::Quantum) == 0 &&
        circ.n_out_edges_of_type(v, EdgeType::Classical) == 0 &&
        circ.n_out_edges_of_type(v, EdgeType::WASM) == 0 &&
        circ.n_out_edges_of_type(v, EdgeType::RNG) == 0) {
      cut_.slice->push_back(v);
    }
  }
}

SliceIterator::SliceIterator(
    const Circuit& circ, const std::function<bool(Op_ptr)>& skip_func)
    : cut_(), circ_(&circ) {
  cut_.init();
  // add qubits to u_frontier
  for (const Qubit& q : circ.all_qubits()) {
    Vertex in = circ.get_in(q);
    cut_.u_frontier->insert({q, circ.get_nth_out_edge(in, 0)});
  }

  // add bits to u_frontier and b_frontier
  for (const Bit& b : circ.all_bits()) {
    Vertex in = circ.get_in(b);
    cut_.b_frontier->insert({b, circ.get_nth_b_out_bundle(in, 0)});
    cut_.u_frontier->insert({b, circ.get_nth_out_edge(in, 0)});
  }

  // add WasmState to u_frontier
  for (unsigned i = 0; i < circ._number_of_wasm_wires; ++i) {
    Vertex in = circ.get_in(circ.wasmwire[i]);
    cut_.slice->push_back(in);
    cut_.u_frontier->insert({circ.wasmwire[i], circ.get_nth_out_edge(in, 0)});
  }

  // add RngState to u_frontier
  for (unsigned i = 0; i < circ._number_of_rng_wires; ++i) {
    Vertex in = circ.get_in(circ.rngwire[i]);
    cut_.slice->push_back(in);
    cut_.u_frontier->insert({circ.rngwire[i], circ.get_nth_out_edge(in, 0)});
  }

  prev_b_frontier_ = cut_.b_frontier;
  cut_ = circ.next_cut(cut_.u_frontier, cut_.b_frontier, skip_func);
}

SliceIterator Circuit::slice_begin() const { return SliceIterator(*this); }

SliceIterator Circuit::slice_end() { return nullsit; }
const SliceIterator Circuit::nullsit = SliceIterator();

SliceIterator::Sliceholder SliceIterator::operator++(int) {
  Sliceholder ret(*cut_.slice);
  ++*this;
  return ret;
}

SliceIterator& SliceIterator::operator++() {
  if (this->finished()) {
    *this = circ_->slice_end();
    return *this;
  }
  prev_b_frontier_ = cut_.b_frontier;
  cut_ = circ_->next_cut(cut_.u_frontier, cut_.b_frontier);
  return *this;
}

bool SliceIterator::finished() const {
  for (const std::pair<UnitID, Edge>& pair :
       this->cut_.u_frontier->get<TagKey>()) {
    if (!circ_->detect_final_Op(circ_->target(pair.second))) return false;
  }
  for (const std::pair<Bit, EdgeVec>& pair :
       this->cut_.b_frontier->get<TagKey>()) {
    if (!pair.second.empty()) return false;
  }
  return true;
}

static std::shared_ptr<unit_frontier_t> get_next_u_frontier(
    const Circuit& circ, std::shared_ptr<const unit_frontier_t> u_frontier,
    const VertexSet& next_slice_lookup) {
  std::shared_ptr<unit_frontier_t> next_frontier =
      std::make_shared<unit_frontier_t>();
  for (const std::pair<UnitID, Edge>& pair : u_frontier->get<TagKey>()) {
    Vertex next_v = circ.target(pair.second);
    if (next_slice_lookup.find(next_v) == next_slice_lookup.end()) {
      next_frontier->insert(pair);
    } else {
      next_frontier->insert(
          {pair.first, circ.get_next_edge(next_v, pair.second)});
    }
  }
  return next_frontier;
}

static std::shared_ptr<b_frontier_t> get_next_b_frontier(
    const Circuit& circ, std::shared_ptr<const b_frontier_t> b_frontier,
    std::shared_ptr<const unit_frontier_t> u_frontier,
    const VertexSet& next_slice_lookup) {
  std::shared_ptr<b_frontier_t> next_b_frontier =
      std::make_shared<b_frontier_t>();
  // Copy any remaining edges
  for (const std::pair<Bit, EdgeVec>& pair : b_frontier->get<TagKey>()) {
    EdgeVec remaining;
    for (const Edge& e : pair.second) {
      Vertex targ = circ.target(e);
      if (next_slice_lookup.find(targ) == next_slice_lookup.end()) {
        remaining.push_back(e);
      }
    }
    if (!remaining.empty()) {
      next_b_frontier->insert({pair.first, remaining});
    }
  }
  // Add any new bits introduced in this slice
  for (const std::pair<UnitID, Edge>& pair : u_frontier->get<TagKey>()) {
    switch (circ.get_edgetype(pair.second)) {
      case EdgeType::Quantum:
      case EdgeType::WASM:
      case EdgeType::RNG: {
        break;
      }
      case EdgeType::Classical: {
        Vertex next_v = circ.target(pair.second);
        if (next_slice_lookup.find(next_v) == next_slice_lookup.end()) continue;
        if (next_b_frontier->get<TagKey>().find(Bit(pair.first)) !=
            next_b_frontier->end()) {
          TKET_ASSERT(!"RAW hazard created in slicing");
        }
        port_t p = circ.get_target_port(pair.second);
        EdgeVec reads = circ.get_nth_b_out_bundle(next_v, p);
        if (!reads.empty()) next_b_frontier->insert({Bit(pair.first), reads});
        break;
      }
      case EdgeType::Boolean: {
        throw CircuitInvalidity("Boolean edge not allowed in b_frontier_t");
      }
      default: {
        TKET_ASSERT(!"get_next_b_frontier found invalid edge type in sig");
      }
    }
  }
  return next_b_frontier;
}

// Utility container for an extendable list of edges with fast membership
// checking.
class EdgeBundle {
 public:
  EdgeBundle() : edge_vec(), edge_set() {}
  void add(const Edge& e) {
    edge_vec.push_back(e);
    edge_set.insert(e);
  }
  bool contains(const Edge& e) const { return edge_set.contains(e); }
  const EdgeVec& get() const { return edge_vec; }
  void clear() {
    edge_vec.clear();
    edge_set.clear();
  }

 private:
  EdgeVec edge_vec;
  EdgeSet edge_set;
};

// Return all the edges contained in the two frontiers, except for any classical
// edges in the unit frontier whose bit also has an edge in the boolean frontier
// with a different target.
static EdgeBundle next_edges_from_frontiers(
    const Circuit& circ, std::shared_ptr<const unit_frontier_t> u_frontier,
    std::shared_ptr<const b_frontier_t> b_frontier) {
  EdgeBundle edges;
  for (const std::pair<UnitID, Edge>& pair : u_frontier->get<TagKey>()) {
    const UnitID& unit = pair.first;
    const Edge& e = pair.second;
    if (unit.type() == UnitType::Bit) {
      Vertex targ = circ.target(e);
      b_frontier_t::const_iterator found =
          b_frontier->get<TagKey>().find(Bit(unit));
      if (found != b_frontier->get<TagKey>().end()) {
        auto b_edges = found->second;
        if (std::any_of(
                b_edges.begin(), b_edges.end(),
                [&circ, &targ](const Edge& b_edge) {
                  return circ.target(b_edge) != targ;
                })) {
          continue;
        }
      }
    }
    edges.add(e);
  }
  for (const std::pair<Bit, EdgeVec>& pair : b_frontier->get<TagKey>()) {
    for (const Edge& e : pair.second) {
      edges.add(e);
    }
  }
  return edges;
}

CutFrontier Circuit::next_cut(
    std::shared_ptr<const unit_frontier_t> u_frontier,
    std::shared_ptr<const b_frontier_t> b_frontier,
    const std::function<bool(Op_ptr)>& skip_func) const {
  EdgeBundle all_edges =
      next_edges_from_frontiers(*this, u_frontier, b_frontier);

  VertexSet bad_vertices;
  if (skip_func) {
    // advance through skippable
    bool can_skip;
    do {
      can_skip = false;
      VertexSet skip_slice_lookup;
      for (const Edge& e : all_edges.get()) {
        Vertex try_v = target(e);
        if (detect_final_Op(try_v) ||
            (!skip_func(get_Op_ptr_from_Vertex(try_v))))
          continue;
        if (skip_slice_lookup.find(try_v) != skip_slice_lookup.end()) continue;
        bool good_vertex = bad_vertices.find(try_v) == bad_vertices.end();
        if (!good_vertex) continue;
        const EdgeVec ins = get_in_edges(try_v);
        for (const Edge& in : ins) {
          if (!all_edges.contains(in)) {
            good_vertex = false;
            break;
          }
        }
        if (!good_vertex) {
          bad_vertices.insert(try_v);
          continue;
        }
        skip_slice_lookup.insert(try_v);
      }
      if (!skip_slice_lookup.empty()) {
        b_frontier = get_next_b_frontier(
            *this, b_frontier, u_frontier, skip_slice_lookup);
        u_frontier = get_next_u_frontier(*this, u_frontier, skip_slice_lookup);
        bad_vertices = {};
        all_edges.clear();

        for (const std::pair<UnitID, Edge>& pair : u_frontier->get<TagKey>()) {
          Edge e = pair.second;
          all_edges.add(e);
        }
        for (const std::pair<Bit, EdgeVec>& pair : b_frontier->get<TagKey>()) {
          for (const Edge& edge : pair.second) {
            Edge e = edge;
            all_edges.add(e);
          }
        }
        can_skip = true;
      }
    } while (can_skip);
  }

  // find the next slice first
  auto next_slice = std::make_shared<Slice>();
  VertexSet next_slice_lookup;
  for (const Edge& e : all_edges.get()) {
    Vertex try_v = target(e);
    if (detect_final_Op(try_v)) continue;
    if (next_slice_lookup.find(try_v) != next_slice_lookup.end())
      continue;  // already going to be in next slice
    bool good_vertex = bad_vertices.find(try_v) == bad_vertices.end();
    if (!good_vertex) continue;
    const EdgeVec ins = get_in_edges(try_v);
    for (const Edge& in : ins) {
      if (!all_edges.contains(in)) {
        good_vertex = false;
        bad_vertices.insert(try_v);
        break;
      }
    }
    if (good_vertex) {
      next_slice_lookup.insert(try_v);
      next_slice->push_back(try_v);
    }
  }
  return {
      next_slice, get_next_u_frontier(*this, u_frontier, next_slice_lookup),
      get_next_b_frontier(*this, b_frontier, u_frontier, next_slice_lookup)};
}

CutFrontier Circuit::next_q_cut(
    std::shared_ptr<const unit_frontier_t> u_frontier) const {
  auto next_slice = std::make_shared<Slice>();
  VertexSet next_slice_lookup;
  VertexSet bad_vertices;
  EdgeSet edge_lookup;
  for (const std::pair<UnitID, Edge>& pair : u_frontier->get<TagKey>()) {
    edge_lookup.insert(pair.second);
  }

  // find the next slice first
  for (const std::pair<UnitID, Edge>& pair : u_frontier->get<TagKey>()) {
    Vertex try_v = target(pair.second);
    if (detect_final_Op(try_v)) continue;
    if (next_slice_lookup.contains(try_v))
      continue;  // already going to be in next slice
    bool good_vertex = !bad_vertices.contains(try_v);
    if (!good_vertex) continue;
    EdgeVec ins = get_in_edges(try_v);
    for (const Edge& in : ins) {
      if (!edge_lookup.contains(in) && (get_edgetype(in) == EdgeType::Quantum ||
                                        get_edgetype(in) == EdgeType::WASM ||
                                        get_edgetype(in) == EdgeType::RNG)) {
        good_vertex = false;
        bad_vertices.insert(try_v);
        break;
      }
    }
    if (good_vertex) {
      next_slice_lookup.insert(try_v);
      next_slice->push_back(try_v);
    }
  }

  return {
      next_slice, get_next_u_frontier(*this, u_frontier, next_slice_lookup),
      std::make_shared<b_frontier_t>()};
}

}  // namespace tket
