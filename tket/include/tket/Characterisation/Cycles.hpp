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

#pragma once

#include <unordered_map>

#include "tket/Circuit/Circuit.hpp"

namespace tket {

typedef std::pair<Edge, Edge> edge_pair_t;

// CycleCom stores minimum command information required for FrameRandomisation
struct CycleCom {
  OpType type;
  std::vector<unsigned> indices;
  Vertex address;

  bool operator==(const CycleCom& other) const {
    return this->type == other.type && this->indices == other.indices;
  }
};

// Cycle stores minimum subcircuit information required for FrameRandomisation
class Cycle {
 public:
  Cycle() {}
  Cycle(
      const std::vector<edge_pair_t>& _boundary_edges,
      const std::vector<CycleCom>& _coms)
      : boundary_edges_(_boundary_edges), coms_(_coms) {}

  // Returns size of boundary_edges_
  unsigned size() const;

  // For edge_pair_t ep in boundary_edges_, if ep.first == source_edge,
  // ep.second = replacement_edge
  void update_boundary(const Edge& source_edge, const Edge& replacement_edge);

  // Adds coms_ from new_cycle to end of this->coms_
  // Extends this->boundary_edges_ with new_cycles.boundary_edges_
  void merge(Cycle& new_cycle);

  // if CycleCom.indices has unsigned equal to key in new_indices, change to
  // value
  void update_coms_indices(const std::map<unsigned, unsigned>& new_indices);

  // Cycle has noop vertices wired into edges in _boundary_edges for purpose of
  // Frame Randomisation This method these vertices in frame_vertices
  void add_vertex_pair(const std::pair<Vertex, Vertex>& verts);

  // Returns frame_vertices
  std::vector<std::pair<Vertex, Vertex>> get_frame() const;

  bool operator==(const Cycle& other) const {
    return this->size() == other.size() && this->coms_ == other.coms_;
  }

  std::vector<edge_pair_t> boundary_edges_;
  std::vector<CycleCom> coms_;

 private:
  std::vector<std::pair<Vertex, Vertex>> frame_vertices;
};

// CycleHistory stores information used to find minimal number of boundaries
struct CycleHistory {
  // Every time a new cycle is made it's assigned a key of type unsigned
  // The size of key is used to track the causal ordering of cycles
  unsigned key;
  // history.size() == key always
  // Tracks which UnitID were in which Cycle
  std::vector<std::vector<UnitID>> history;
  // Map from UnitID to key, where key maps in key_to_cycle to "active" Cycle
  std::map<UnitID, unsigned> uid_to_key;
  // Map from key to cycle, where a key n refers to the nth cycle made
  std::map<unsigned, Cycle> key_to_cycle;
};

class CycleFinder {
 public:
  CycleFinder(const Circuit& _circ, const OpTypeSet& _cycle_types)
      : circ(_circ), cycle_types_(_cycle_types) {};

  // Cycles are sub-circuits of circ where every gate has OpType in cycle_types_
  // get_cycles() returns the minimum number of cycles such that every
  // cycle_types_ gate in circ is in one cycle
  std::vector<Cycle> get_cycles();

 private:
  // Circuit cycles are found in
  const Circuit& circ;
  // OpType cycle gates have
  const OpTypeSet cycle_types_;

  // CycleFinder uses SliceIterator to find get circ gates in cycle_types_
  // In Edges to a new slice are checked for equality with the last boundary out
  // edge stored for their UnitID If equal a cycle may be extended, if not equal
  std::map<Edge, UnitID> cycle_out_edges;

  // Stores data structures for tracking created Cycles and associated UnitID's
  CycleHistory cycle_history;
  std::unordered_map<Vertex, unsigned> vertex_to_cycle_key;
  std::map<unsigned, VertexVec> cycle_non_quantum_successors;

  // Given a new CutFrontier object, creates new cycles from interior vertices
  // and merges them with previous cycles where possible
  void extend_cycles(const CutFrontier& cut);

  // Makes a new Cycle object from given dag information
  // Return type used to identify whether cycle should be merged with previous
  // cycles
  std::pair<unsigned, std::set<unsigned>> make_cycle(
      const Vertex& v, const EdgeVec& out_edges, const CutFrontier& cut);

  // Uses CycleHistory to merge Cycle attributed to new_key with Cycles
  // attributed to old_keys Cycles are merged in to Cycle with smallest key
  void merge_cycles(
      unsigned new_key, std::set<unsigned>& old_keys,
      const VertexSet& frontier_targets);

  // Check for a path that leaves the proposed merge and returns to it.
  bool merge_is_acyclic(
      const std::set<unsigned>& keys, const VertexSet& frontier_targets) const;

  void append_cycle_successors(unsigned key, VertexVec& successors) const;

  // Updates cycle_out_edges with new Edge
  // UnitID should not be new, so error thrown if not found
  void update_cycle_out_edges(const UnitID& uid, const Edge& e);

  // Getter for UnitID from unit_frontier_t in Cut
  UnitID unitid_from_unit_frontier(
      const std::shared_ptr<unit_frontier_t>& u_frontier, const Edge& e) const;

  // if unsigned in erase_from is <= to_erase, removes from erase_from
  void erase_keys(
      const unsigned& to_erase, std::set<unsigned>& erase_from) const;

  // removes unsigned from old_keys that have cycles that can't merge with
  // new_key adds new_key to old_keys
  void order_keys(unsigned& new_key, std::set<unsigned>& old_keys) const;
};
}  // namespace tket
