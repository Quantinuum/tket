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

#include "tket/Characterisation/Cycles.hpp"

#include <numeric>

#include "tket/Circuit/Slices.hpp"

namespace tket {

class CycleError : public std::logic_error {
 public:
  explicit CycleError(const std::string& message) : std::logic_error(message) {}
};

unsigned Cycle::size() const { return boundary_edges_.size(); }

void Cycle::add_vertex_pair(const std::pair<Vertex, Vertex>& verts) {
  frame_vertices.push_back(verts);
}

std::vector<std::pair<Vertex, Vertex>> Cycle::get_frame() const {
  return frame_vertices;
}

UnitID CycleFinder::unitid_from_unit_frontier(
    const std::shared_ptr<unit_frontier_t>& u_frontier, const Edge& e) const {
  for (const std::pair<UnitID, Edge>& pair : u_frontier->get<TagKey>()) {
    if (pair.second == e) {
      return pair.first;
    }
  }
  throw CycleError(std::string("Edge not in unit_frontier_t object."));
}

void CycleFinder::update_cycle_out_edges(const UnitID& uid, const Edge& e) {
  for (std::pair<const Edge, UnitID>& pair : cycle_out_edges) {
    if (pair.second == uid) {
      cycle_out_edges.erase(pair.first);
      cycle_out_edges[e] = uid;
      return;
    }
  }
  throw CycleError(
      std::string(
          "UnitID " + uid.repr() + " not in std::map<Edge, UnitID> object."));
  return;
}

void Cycle::update_boundary(
    const Edge& source_edge, const Edge& replacement_edge) {
  for (unsigned i = 0; i < boundary_edges_.size(); i++) {
    if (boundary_edges_[i].second == source_edge) {
      boundary_edges_[i].second = replacement_edge;
      return;
    }
  }
  throw CycleError("Source Edge matches no edges in boundary to cycle.");
  return;
}

void CycleFinder::erase_keys(
    const unsigned& to_erase, std::set<unsigned>& erase_from) const {
  std::set<unsigned> all_erased;
  for (const unsigned& key : erase_from) {
    if (key <= to_erase) {
      all_erased.insert(key);
    }
  }
  for (const unsigned& key : all_erased) erase_from.erase(key);
}

// Adds a new cycle to this->cycle_history.key_to_cycle
std::pair<unsigned, std::set<unsigned>> CycleFinder::make_cycle(
    const Vertex& v, const EdgeVec& out_edges, const CutFrontier& cut) {
  // Make a new boundary, add it to "all_boundaries", update "boundary_key" map
  std::vector<edge_pair_t> new_cycle_boundary;
  std::set<UnitID> new_boundary_uids;
  std::set<unsigned> old_boundary_keys;
  std::set<unsigned> not_mergeable_keys;
  std::set<UnitID> not_mergeable_uids;

  // Get UnitID, add to uids for new boundary, keep note of all old boundary
  // keys for merging set new boundary key, add new edges to new boundary update
  // cycle out edges if the edge can't be found

  for (const Edge& e : out_edges) {
    UnitID uid = unitid_from_unit_frontier(cut.u_frontier, e);
    new_boundary_uids.insert(uid);
    old_boundary_keys.insert(this->cycle_history.uid_to_key[uid]);
    // boundary key associated with given edge e not included
    // i.e. e's associated in edge isn't a cycle out edge
    // i.e. some other non-cycle gate has been passed

    // if the edge going into the vertex is not in a previous cycle
    // then it can't be merged with that cycle
    if (cycle_out_edges.find(circ.get_last_edge(v, e)) ==
        cycle_out_edges.end()) {
      not_mergeable_keys.insert(this->cycle_history.uid_to_key[uid]);
    }
    this->cycle_history.uid_to_key[uid] = this->cycle_history.key;
    new_cycle_boundary.push_back({circ.get_last_edge(v, e), e});
    update_cycle_out_edges(uid, e);
  }

  for (const unsigned& key : old_boundary_keys) {
    for (const UnitID& uid : this->cycle_history.history[key]) {
      if (not_mergeable_uids.find(uid) != not_mergeable_uids.end()) {
        not_mergeable_keys.insert(key);
        break;
      }
    }
  }

  std::vector<unsigned> op_indices(out_edges.size());
  std::iota(op_indices.begin(), op_indices.end(), 0);
  // Edges in port ordering
  Cycle new_cycle(
      new_cycle_boundary, {{circ.get_OpType_from_Vertex(v), op_indices, v}});
  this->cycle_history.key_to_cycle[this->cycle_history.key] = {new_cycle};

  this->cycle_history.history.push_back(
      std::vector<UnitID>(new_boundary_uids.begin(), new_boundary_uids.end()));
  for (const unsigned& key : not_mergeable_keys) {
    erase_keys(key, old_boundary_keys);
  }

  not_mergeable_keys.clear();

  // For each candidate cycle "cycle" given as an unsigned key
  // For all UnitID "uid" in "v" that are not in "cycle"
  // For all cycles between cycle_history.uid_to_key[uid] and "cycle"
  // If a cycle contains both "uid" and any "uid" from "cycle"
  // Set the key as not to be merged
  for (const unsigned& candidate_cycle_key : old_boundary_keys) {
    std::vector<UnitID> candidate_cycle_uid =
        this->cycle_history.history[candidate_cycle_key];
    std::set<UnitID> not_present_uids;
    for (const UnitID& candidate_uids : new_boundary_uids) {
      if (std::find(
              candidate_cycle_uid.begin(), candidate_cycle_uid.end(),
              candidate_uids) == candidate_cycle_uid.end()) {
        not_present_uids.insert(candidate_uids);
      }
    }
    // not_present_uids now contains UnitID in the new cycle that are not in the
    // cycle represented by "candidate_cycle_key"

    // We now iterate through all cycles between these UnitIDs' most recent
    // cycles and our target cycle

    // If any of these cycles have both a non-present uid and any uid in the
    // target cycle then this key is not mergeable and would lead to a "cycle in
    // the DAG" (different type of cycle)
    for (const UnitID& not_present_uid : not_present_uids) {
      for (unsigned i = candidate_cycle_key;
           i < this->cycle_history.uid_to_key[not_present_uid]; i++) {
        std::vector<UnitID> history_uids = this->cycle_history.history[i];
        if (std::find(
                history_uids.begin(), history_uids.end(), not_present_uid) !=
            history_uids.end()) {
          std::vector<UnitID> intersection;
          std::vector<UnitID> candidates =
              cycle_history.history[candidate_cycle_key];
          std::set_intersection(
              candidates.begin(), candidates.end(), history_uids.begin(),
              history_uids.end(), std::back_inserter(intersection));
          if (!intersection.empty()) {
            not_mergeable_keys.insert(candidate_cycle_key);
            break;
          }
        }
      }
    }
  }

  for (const unsigned& key : not_mergeable_keys) {
    erase_keys(key, old_boundary_keys);
  }

  std::pair<unsigned, std::set<unsigned>> return_keys = {
      this->cycle_history.key, old_boundary_keys};
  this->cycle_history.key++;
  return return_keys;
}

// "old_keys" returned gives keys for boundaries to be merged together
// these "old_keys" are keys corresponding to boundaries UnitIDs in new boundary
// were in previously if two keys in passed old_keys have overlapping UnitIDs,
// then they cannot be merged in this case, remove the 'earlier' boundary from
// keys to be merged
void CycleFinder::order_keys(
    unsigned& new_key, std::set<unsigned>& old_keys) const {
  std::set<unsigned> bad_keys;
  for (std::set<unsigned>::iterator it = old_keys.begin(); it != old_keys.end();
       ++it) {
    std::set<unsigned>::iterator jt = it;
    std::advance(jt, 1);
    // if either std::vector<UnitID> has common UnitIDs then one is causally
    // blocked. remove smaller key from keys from set as not a candidate for
    // merging into
    for (; jt != old_keys.end(); ++jt)
      for (const UnitID& u0 : this->cycle_history.history[*it])
        for (const UnitID& u1 : this->cycle_history.history[*jt])
          if (u0 == u1) {
            bad_keys.insert(*it);
            goto new_loop;
          }

  new_loop:
    {
    }
  }

  for (const unsigned& key : bad_keys) old_keys.erase(key);
  old_keys.insert(new_key);
}

void Cycle::update_coms_indices(
    const std::map<unsigned, unsigned>& new_indices) {
  for (CycleCom& com : coms_) {
    for (unsigned& index : com.indices) {
      index = new_indices.at(index);
    }
  }
}

// new cycle is made
// it may need to be immediately merged into other cycles
// 1) check for overlap of UnitID between cycles of "old_keys"
// if overlap, remove lowest value key from set
// 2) add "new_key" to "old_keys", merge all boundaries into corresponding
// boundary to highest value "old_key"
void Cycle::merge(Cycle& new_cycle) {
  // iterate through edges in boundary of new_cycle
  std::map<unsigned, unsigned> new_indices;
  for (unsigned i = 0; i < new_cycle.boundary_edges_.size(); i++) {
    bool match = false;
    for (unsigned j = 0; j < boundary_edges_.size(); j++) {
      // if in edge of boundary of new_cycle matches out of edge *this, update
      // *this out edge
      if (boundary_edges_[j].second == new_cycle.boundary_edges_[i].first) {
        boundary_edges_[j].second = new_cycle.boundary_edges_[i].second;
        // As CycleCom are labelled by basic indexing system, where indexing is
        // related to position edge has in boundary Update CycleCom in new_cycle
        // s.t. edges with index i now have index j update commands in new_cycle
        // to match edgesindexing of *this for now, store new indexing
        new_indices[i] = j;
        match = true;
        break;
      }
    }
    if (!match) {
      boundary_edges_.push_back(new_cycle.boundary_edges_[i]);
      new_indices[i] = boundary_edges_.size() - 1;
    }
  }
  // update CycleCom indices in new_cycle
  new_cycle.update_coms_indices(new_indices);
  coms_.insert(coms_.end(), new_cycle.coms_.begin(), new_cycle.coms_.end());
}

void CycleFinder::merge_cycles(unsigned new_key, std::set<unsigned>& old_keys) {
  // boundary_keys is a std::set<unsigned> so is ordered
  order_keys(new_key, old_keys);
  // all cycles corresponding to keys in boundary_keys should be merged into
  // boundary with smallest value
  std::set<unsigned>::iterator it = old_keys.begin();
  unsigned base_key = *it;
  std::advance(it, 1);
  for (; it != old_keys.end(); ++it) {
    unsigned merge_key = *it;
    this->cycle_history.key_to_cycle[base_key].merge(
        this->cycle_history.key_to_cycle[merge_key]);
    this->cycle_history.key_to_cycle.erase(merge_key);
    // update this->cycle_history.history for "base_key"
    for (const UnitID& u0 : this->cycle_history.history[merge_key]) {
      this->cycle_history.uid_to_key[u0] = base_key;
      for (const UnitID& u1 : this->cycle_history.history[base_key])
        if (u0 == u1) break;
      this->cycle_history.history[base_key].push_back(u0);
    }
  }
}

void CycleFinder::extend_cycles(const CutFrontier& cut) {
  // For each vertex in slice
  // Make a new "cycle"
  // If any in edge to new cycle matches an out edge to a previous cycle,
  // attempt to merge cycles together
  for (const Vertex& v : *cut.slice) {
    EdgeVec out_edges = circ.get_out_edges_of_type(v, EdgeType::Quantum);
    // Compare EdgeType::Quantum "in_edges" of vertex in slice to collection of
    // out edges from "active" boundaries If an "in_edge" is not equivalent to
    // an out edge from "active" boundary, implies vertex needs to start a new
    // boundary
    std::pair<unsigned, std::set<unsigned>> all_keys =
        make_cycle(v, out_edges, cut);
    if (all_keys.second.size() > 0) {
      merge_cycles(all_keys.first, all_keys.second);
    }
  }
}

std::vector<Cycle> CycleFinder::get_cycles() {
  std::function<bool(Op_ptr)> skip_func = [&](Op_ptr op) {
    return (cycle_types_.find(op->get_type()) == cycle_types_.end());
  };

  SliceIterator slice_iter(circ, skip_func);
  this->cycle_history.key = 0;

  // initialization
  if (!(*slice_iter).empty()) {
    for (const std::pair<UnitID, Edge>& pair :
         slice_iter.cut_.u_frontier->get<TagKey>()) {
      Edge in_edge = pair.second;
      if (circ.get_edgetype(in_edge) != EdgeType::Classical) {
        Vertex in_vert = circ.source(pair.second);
        // if vertex is type from types, then vertex is in slice and need
        // in_edge. else can ignore.
        if (cycle_types_.find(circ.get_OpType_from_Vertex(in_vert)) !=
            cycle_types_.end()) {
          in_edge = circ.get_last_edge(in_vert, pair.second);
        }
        cycle_out_edges.insert({in_edge, pair.first});
        this->cycle_history.uid_to_key.insert(
            {pair.first, this->cycle_history.key});
        Cycle new_cycle({{in_edge, in_edge}}, {{}});
        this->cycle_history.key_to_cycle[this->cycle_history.key] = new_cycle;
        this->cycle_history.history.push_back({pair.first});
        this->cycle_history.key++;
      }
    }
    // extend cycles automatically merges cycles that can be merge due to
    // overlapping multi-qubit gates
    this->extend_cycles(slice_iter.cut_);
    cycle_out_edges = {};
    for (const std::pair<UnitID, Edge>& pair :
         slice_iter.cut_.u_frontier->get<TagKey>()) {
      cycle_out_edges.insert({pair.second, pair.first});
    }
  }
  while (!slice_iter.finished()) {
    slice_iter.cut_ = circ.next_cut(
        slice_iter.cut_.u_frontier, slice_iter.cut_.b_frontier, skip_func);
    if (!(*slice_iter).empty()) {
      this->extend_cycles(slice_iter.cut_);
    }
  }

  // Skim Cycle type from CycleHistory.key_to_cycle
  // Discard any Cycles with only Input Gates
  std::vector<Cycle> output_cycles;
  for (std::pair<unsigned, Cycle> entry : this->cycle_history.key_to_cycle) {
    if (entry.second.coms_.size() == 0) {
      throw CycleError(std::string("Cycle with no internal gates."));
    }
    if (entry.second.boundary_edges_[0].first ==
        entry.second.boundary_edges_[0].second) {
      continue;
    }
    if (entry.second.size() > circ.n_qubits()) {
      throw CycleError(
          std::string("Cycle has a larger frame than Circuit has qubits."));
    }
    output_cycles.push_back(entry.second);
  }
  return output_cycles;
}
}  // namespace tket
