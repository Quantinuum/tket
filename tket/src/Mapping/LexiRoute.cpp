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

#include "tket/Mapping/LexiRoute.hpp"

#include "tket/Mapping/MappingFrontier.hpp"

namespace tket {

LexiRoute::LexiRoute(
    const ArchitecturePtr& _architecture,
    MappingFrontier_ptr& _mapping_frontier)
    : architecture_(_architecture), mapping_frontier_(_mapping_frontier) {
  // set initial logical->physical labelling
  for (const Qubit& qb : this->mapping_frontier_->circuit_.all_qubits()) {
    this->labelling_.insert({qb, qb});
    Node n(qb);
    // store which Node have been assigned to Circuit already
    if (this->architecture_->node_exists(n)) {
      this->assigned_nodes_.insert(n);
    }
  }
}

void LexiRoute::reassign_to_any_ancilla_node(const Node& reassign_node) {
  /**
   * Find any ancilla node.
   * Merge the back of the ancilla path with the start of the
   * "reassign_node" path.
   * We know that "reassign_node" is for a Qubit wire with no
   * multi-qubit gates.
   * Remove as a known ancilla node and update the Circuit.
   */

  TKET_ASSERT(!this->mapping_frontier_->ancilla_nodes_.empty());
  auto ancilla_it = this->mapping_frontier_->ancilla_nodes_.begin();
  Node ancilla_node = *ancilla_it;
  this->mapping_frontier_->ancilla_nodes_.erase(ancilla_it);
  this->mapping_frontier_->merge_ancilla(reassign_node, ancilla_node);
  auto it = std::find_if(
      this->labelling_.begin(), this->labelling_.end(),
      [&reassign_node](const std::pair<const UnitID, UnitID>& pair_unitid) {
        return pair_unitid.second == reassign_node;
      });
  TKET_ASSERT(it != this->labelling_.end());
  this->labelling_[it->first] = ancilla_node;
  unit_map_t relabel = {{UnitID(reassign_node), UnitID(ancilla_node)}};
  this->mapping_frontier_->circuit_.rename_units(relabel);
}

bool LexiRoute::reassign_to_any_spare_node(const Node& reassign_node) {
  /**
   * First find an Architecture Node that is not already being used to label
   * a Circuit wire.
   * If there are no unused Node, return false.
   */
  std::set<Node> all_nodes_s;
  for (const std::pair<const UnitID, UnitID>& p : this->labelling_) {
    all_nodes_s.insert(Node(p.second));
  }
  std::set<Node> architecture_nodes = this->architecture_->nodes();
  auto it = std::find_if(
      architecture_nodes.begin(), architecture_nodes.end(),
      [&all_nodes_s](const Node& node) {
        return all_nodes_s.find(node) == all_nodes_s.end();
      });

  if (it == architecture_nodes.end()) {
    return false;
  }

  /**
   * Update this->labelling_.
   * "reassign_node" will already have an entry
   * in this->labelling_, as a relabelling of an original Circuit UnitID.
   * Find this->labelling_, remove it, and add a new labelling between
   * whatever this original Circuit UnitID is and the found spare node.
   *
   */
  Node spare_node = *it;

  auto jt = std::find_if(
      this->labelling_.begin(), this->labelling_.end(),
      [&reassign_node](const std::pair<const UnitID, UnitID>& pair_unitid) {
        return pair_unitid.second == reassign_node;
      });
  TKET_ASSERT(jt != this->labelling_.end());
  this->labelling_[jt->first] = spare_node;
  /**
   * Update set of "reassignable nodes" to know that "spare_node"
   * is now reassignable.
   */
  auto reassign_node_reassignable_it =
      this->mapping_frontier_->reassignable_nodes_.find(reassign_node);
  TKET_ASSERT(
      reassign_node_reassignable_it !=
      this->mapping_frontier_->reassignable_nodes_.end());
  this->mapping_frontier_->reassignable_nodes_.erase(
      reassign_node_reassignable_it);
  this->mapping_frontier_->reassignable_nodes_.insert(spare_node);

  /**
   * Update linear boundary so that the wire for "reassign_node"
   * is now labelled as "spare_node"
   */
  auto reassign_node_boundary_it =
      this->mapping_frontier_->linear_boundary->get<TagKey>().find(
          reassign_node);
  TKET_ASSERT(
      reassign_node_boundary_it !=
      this->mapping_frontier_->linear_boundary->get<TagKey>().end());
  this->mapping_frontier_->linear_boundary->replace(
      reassign_node_boundary_it,
      {spare_node, reassign_node_boundary_it->second});

  /**
   * Update the Circuit object so that the wire for "reassign_node" is now
   * "spare_node"
   */
  unit_map_t relabel = {{UnitID(reassign_node), UnitID(spare_node)}};
  this->mapping_frontier_->circuit_.rename_units(relabel);

  /**
   * Update bimaps such that where things were known to be labelled as
   * "reassign_node" are now known as "spare_node"
   */
  auto initial_reassign_it =
      this->mapping_frontier_->bimaps_->initial.right.find(reassign_node);
  TKET_ASSERT(
      initial_reassign_it !=
      this->mapping_frontier_->bimaps_->initial.right.end());
  UnitID preserved_orig = initial_reassign_it->second;
  std::pair<UnitID, UnitID> initial_replacement = {preserved_orig, spare_node};
  this->mapping_frontier_->bimaps_->initial.right.erase(initial_reassign_it);
  this->mapping_frontier_->bimaps_->initial.left.insert(initial_replacement);
  auto final_reassign_it =
      this->mapping_frontier_->bimaps_->final.left.find(preserved_orig);
  TKET_ASSERT(
      final_reassign_it != this->mapping_frontier_->bimaps_->final.left.end());
  std::pair<UnitID, UnitID> final_replacement = {preserved_orig, spare_node};
  this->mapping_frontier_->bimaps_->final.left.erase(final_reassign_it);
  this->mapping_frontier_->bimaps_->final.left.insert(final_replacement);

  /**
   * Set "spare_node" to be a know assigned node in the Circuit
   */
  this->assigned_nodes_.insert(spare_node);
  return true;
}

void LexiRoute::reassign_node(
    const Node& reassign_node, const UnitID& assignee) {
  /**
   * A "reassignable" node has no relevant use for the purposes of
   * routing.
   * It is assigned  at the start of routing to avoid causal constraints from
   * Barriers and other classical controlled but not quantum vertices
   *
   * We first find a replacement Architecture Node for "reassign_node" (the
   * reassignable node) to be relabelled to.
   *
   * First we check for any unused Architecture Node we can use to relabel with.
   * If there are none we use an ancilla Node, removing it as "ancillary" and
   * merging the end of the ancilla path with the start of the "reassignable"
   * (i.e. reassign_node) path.
   */
  if (!reassign_to_any_spare_node(reassign_node)) {
    reassign_to_any_ancilla_node(reassign_node);
  }

  /**
   * Regardless of whether a spare or ancilla node is used, the "assignee"
   * UnitID in the Circuit is being relabelled as "reassign_node" and so we
   * update attributes, first the labelling.
   */

  this->labelling_[assignee] = reassign_node;
  /**
   * Update linear boundary, reflecting that the wire corresponding to
   * "assignee" is now labelled as "reassign_node"
   */
  auto assignee_boundary_it =
      this->mapping_frontier_->linear_boundary->get<TagKey>().find(assignee);
  TKET_ASSERT(
      assignee_boundary_it !=
      this->mapping_frontier_->linear_boundary->get<TagKey>().end());
  this->mapping_frontier_->linear_boundary->replace(
      assignee_boundary_it, {reassign_node, assignee_boundary_it->second});

  /**
   * Update the Circuit UnitID, such that the wire for "assignee" is now
   * "reassign_node"
   */
  unit_map_t relabel = {{UnitID(assignee), UnitID(reassign_node)}};
  this->mapping_frontier_->circuit_.rename_units(relabel);

  /** Update bimaps such that what was previously relabelled as "assignee" is
   * now relabelled as "erassign_node"
   *
   */
  auto initial_assignee_it =
      this->mapping_frontier_->bimaps_->initial.right.find(assignee);
  TKET_ASSERT(
      initial_assignee_it !=
      this->mapping_frontier_->bimaps_->initial.right.end());
  UnitID preserved_orig = initial_assignee_it->second;
  std::pair<UnitID, UnitID> initial_replacement = {
      preserved_orig, reassign_node};
  this->mapping_frontier_->bimaps_->initial.right.erase(initial_assignee_it);
  this->mapping_frontier_->bimaps_->initial.left.insert(initial_replacement);
  auto final_assignee_it =
      this->mapping_frontier_->bimaps_->final.left.find(preserved_orig);
  TKET_ASSERT(
      final_assignee_it != this->mapping_frontier_->bimaps_->final.left.end());
  std::pair<UnitID, UnitID> final_replacement = {preserved_orig, reassign_node};
  this->mapping_frontier_->bimaps_->final.left.erase(final_assignee_it);
  this->mapping_frontier_->bimaps_->final.left.insert(final_replacement);

  this->mapping_frontier_->reassignable_nodes_.erase(reassign_node);
}

// "assignee" is Circuit UnitID being relabelled to UnitID "replacement"
void LexiRoute::assign_valid_node(
    const UnitID& assignee, const UnitID& replacement) {
  /**
   * UnitID "replacement" has three possible contexts for the Circuit:
   * 1) It is already being used in the Circuit, but for a Node with no
   * physical constraints and so we can freely reassign it to some other
   * free node i.e. it's in "reassignable_nodes_"
   * 2) It is already being used in the Circuit as an Ancilla node.
   * In this case we need to attach the causal path of "assignee" to the
   * end of "replacement".
   * 3) It is unused and we can just relabel various MappingFrontier attributes
   */

  if (this->mapping_frontier_->reassignable_nodes_.find(Node(replacement)) !=
      this->mapping_frontier_->reassignable_nodes_.end()) {
    /**
     * We are relabelling "assignee" to a Node that is already in the Circuit,
     * but on a UnitID wire with no multi-qubit gates.
     */
    this->reassign_node(Node(replacement), assignee);
    return;
  }
  if (this->mapping_frontier_->ancilla_nodes_.find(Node(replacement)) !=
      this->mapping_frontier_->ancilla_nodes_.end()) {
    /**
     * We are relablelling "assignee" to a Node that is already in the Circuit,
     * but on a UnitID wire that has acted as an ancillary qubit i.e. the qubit
     * state is known to be identity (logically) and we can merge the qubit
     * paths together.
     */
    this->mapping_frontier_->merge_ancilla(assignee, replacement);
    this->labelling_.erase(replacement);
    this->labelling_[assignee] = replacement;
    return;
  }

  /**
   * We are relabelling "assignee" to a Node not in the circuit and can just
   * update various MappingFrontier attributes to recognise this.
   */
  this->labelling_[assignee] = replacement;
  this->assigned_nodes_.insert(Node(replacement));

  // bimaps are used by CompilationUnit to track how an original Circuit Qubit
  // may be relabelled update this to reflect new assignment first do bimaps
  // "RHS"
  auto initial_assignee_it =
      this->mapping_frontier_->bimaps_->initial.right.find(assignee);
  TKET_ASSERT(
      initial_assignee_it !=
      this->mapping_frontier_->bimaps_->initial.right.end());
  UnitID original = initial_assignee_it->second;
  this->mapping_frontier_->bimaps_->initial.right.erase(initial_assignee_it);
  this->mapping_frontier_->bimaps_->initial.left.insert(
      {original, replacement});

  // then bimaps "LHS"
  auto final_assignee_it =
      this->mapping_frontier_->bimaps_->final.left.find(original);
  TKET_ASSERT(
      final_assignee_it != this->mapping_frontier_->bimaps_->final.left.end());
  this->mapping_frontier_->bimaps_->final.left.erase(final_assignee_it);
  this->mapping_frontier_->bimaps_->final.left.insert({original, replacement});

  // Update linear boundaries
  auto assignee_boundary_it =
      this->mapping_frontier_->linear_boundary->get<TagKey>().find(assignee);
  TKET_ASSERT(
      assignee_boundary_it !=
      this->mapping_frontier_->linear_boundary->get<TagKey>().end());
  this->mapping_frontier_->linear_boundary->replace(
      assignee_boundary_it, {replacement, assignee_boundary_it->second});

  unit_map_t relabel = {{UnitID(assignee), UnitID(replacement)}};
  // Relabel Circuit UnitID
  this->mapping_frontier_->circuit_.rename_units(relabel);
}

bool LexiRoute::assign_at_distance(
    const UnitID& assignee, const Node& root, unsigned distances) {
  /**
   * First we find a set of valid Architecture Node that are a
   * distance "distances" away on the connectivity graph
   * from "root".
   *
   * An Architecture Node is "valid" if it is not already assigned.
   */
  node_set_t valid_nodes;
  for (const Node& neighbour :
       this->architecture_->nodes_at_distance(root, distances)) {
    // A node is unassigned if it's empty or holding an ancilla, or can be
    // reassigned
    if (this->mapping_frontier_->reassignable_nodes_.find(neighbour) !=
            this->mapping_frontier_->reassignable_nodes_.end() ||
        this->assigned_nodes_.find(neighbour) == this->assigned_nodes_.end() ||
        this->mapping_frontier_->ancilla_nodes_.find(neighbour) !=
            this->mapping_frontier_->ancilla_nodes_.end()) {
      valid_nodes.insert(neighbour);
    }
  }

  /**
   * Node will need to be found at a larger distance than "distances"
   */
  if (valid_nodes.empty()) {
    return false;
  }
  auto it = valid_nodes.begin();
  /** early exit to avoid getting distances
   * if only one Node to choose from.
   */
  if (valid_nodes.size() == 1) {
    this->assign_valid_node(assignee, Node(*it));
    return true;
  }
  /**
   * Else we compare how "good" a candidate Node
   * is by looking at lexicographical distances
   */
  lexicographical_distances_t winning_distances =
      this->architecture_->get_distances(*it);
  Node preserved_node = *it;
  ++it;
  while (it != valid_nodes.end()) {
    lexicographical_distances_t comparison_distances =
        this->architecture_->get_distances(*it);
    if (comparison_distances < winning_distances) {
      preserved_node = *it;
      winning_distances = comparison_distances;
    }
    ++it;
  }
  /**
   * assign unplaced Node to "winning" preserved_node
   */
  this->assign_valid_node(assignee, preserved_node);
  return true;
}

bool LexiRoute::update_labelling() {
  // iterate through interacting qubits, assigning them to an Architecture
  // Node if they aren't already
  bool relabelled = false;
  for (const auto& pair : this->interacting_uids_) {
    bool uid_0_exist =
        this->architecture_->node_exists(Node(this->labelling_[pair.first]));
    bool uid_1_exist =
        this->architecture_->node_exists(Node(this->labelling_[pair.second]));
    if (!uid_0_exist || !uid_1_exist) {
      relabelled = true;
    }
    if (!uid_0_exist && !uid_1_exist) {
      /**
       * If neither is assigned then we place one on some spare
       * unassigned Qubit. The unplaced Qubit will then
       * naturally be placed next time update_labelling()
       * is called.
       *
       * If no Nodes are assigned in the Architecture,
       * then we assign it to a "good" Architecture Node.
       *
       * If there are some other assigned, we assign it
       * close to pre-assigned Node.
       */
      if (this->assigned_nodes_.size() == 0) {
        /**
         * To assign this Node, we find a spare
         * Architecture Node with the best averaged distance to other
         * Nodes in the Architecture.
         */

        // Find Architecture Node with best out degree
        std::set<Node> max_degree_nodes =
            this->architecture_->max_degree_nodes();
        auto it = max_degree_nodes.begin();

        /**
         * For each of these Nodes, get the distance to each other
         * Architecture Node.
         *
         * Do a lexicographical comparison between each set
         * of distances.
         *
         * Preserve the distance vector "closest" to all
         * other Node
         */
        lexicographical_distances_t winning_distances =
            this->architecture_->get_distances(*it);
        Node preserved_node = Node(*it);
        ++it;
        for (; it != max_degree_nodes.end(); ++it) {
          lexicographical_distances_t comparison_distances =
              this->architecture_->get_distances(*it);
          if (comparison_distances < winning_distances) {
            preserved_node = Node(*it);
            winning_distances = comparison_distances;
          }
        }
        // assign unplaced Qubit to "best" Node
        this->labelling_[pair.first] = preserved_node;
        this->assigned_nodes_.insert(preserved_node);

        // update bimaps
        auto initial_assignee_it =
            this->mapping_frontier_->bimaps_->initial.right.find(pair.first);
        TKET_ASSERT(
            initial_assignee_it !=
            this->mapping_frontier_->bimaps_->initial.right.end());
        UnitID original = initial_assignee_it->second;
        this->mapping_frontier_->bimaps_->initial.right.erase(
            initial_assignee_it);
        this->mapping_frontier_->bimaps_->initial.left.insert(
            {original, preserved_node});

        auto final_assignee_it =
            this->mapping_frontier_->bimaps_->final.left.find(original);
        this->mapping_frontier_->bimaps_->final.left.erase(final_assignee_it);
        this->mapping_frontier_->bimaps_->final.left.insert(
            {original, preserved_node});

        // this prompts next if statement to place "uid1"
        uid_0_exist = true;

        // update circuit with new labelling
        unit_map_t relabel = {{UnitID(pair.first), UnitID(preserved_node)}};
        this->mapping_frontier_->circuit_.rename_units(relabel);

        // update linear boundary
        auto pair_first_boundary_it =
            this->mapping_frontier_->linear_boundary->get<TagKey>().find(
                pair.first);
        this->mapping_frontier_->linear_boundary->replace(
            pair_first_boundary_it,
            {preserved_node, pair_first_boundary_it->second});

        // given best node, do something
      } else {
        /**
         * Find a Node for uid_0 that is already adjacent to an assigned
         * Node but that has a neighbour with a spare slot (for assigning uid_1
         * to).
         */

        auto root_it = this->assigned_nodes_.begin();
        while (!uid_0_exist && root_it != this->assigned_nodes_.end()) {
          Node root = *root_it;
          uid_0_exist = this->assign_at_distance(pair.first, root, 1);
          ++root_it;
        }
        if (!uid_0_exist) {
          throw LexiRouteError(
              "Unable to assign physical qubit - no free qubits remaining.");
        }
      }
    }
    if (!uid_0_exist && uid_1_exist) {
      Node root(this->labelling_[pair.second]);
      for (unsigned k = 1; k <= this->architecture_->get_diameter(); k++) {
        if (this->assign_at_distance(pair.first, root, k)) {
          uid_0_exist = true;
          break;
        }
      }
      if (!uid_0_exist) {
        throw LexiRouteError(
            "Unable to assign physical qubit - no free qubits remaining.");
      }
    }
    if (uid_0_exist && !uid_1_exist) {
      Node root(this->labelling_[pair.first]);
      for (unsigned k = 1; k <= this->architecture_->get_diameter(); k++) {
        if (this->assign_at_distance(pair.second, root, k)) {
          uid_1_exist = true;
          break;
        }
      }
      if (!uid_1_exist) {
        throw LexiRouteError(
            "Unable to assign physical qubit - no free qubits remaining.");
      }
    }
  }
  return relabelled;
}

/**
 * LexiRoute::set_interacting_uids
 * Updates this->interacting_uids_ with all "interacting" pairs
 * of UnitID in this->mapping_frontier_
 */
bool LexiRoute::set_interacting_uids(
    AssignedOnly assigned_only, CheckRoutingValidity route_check,
    CheckLabellingValidity label_check) {
  // return types
  this->interacting_uids_.clear();
  bool all_placed = true;
  for (auto it =
           this->mapping_frontier_->linear_boundary->get<TagKey>().begin();
       it != this->mapping_frontier_->linear_boundary->get<TagKey>().end();
       ++it) {
    Edge e0 = this->mapping_frontier_->circuit_.get_nth_out_edge(
        it->second.first, it->second.second);
    Vertex v0 = this->mapping_frontier_->circuit_.target(e0);
    // should never be input vertex, so can always use in_edges
    Op_ptr op = this->mapping_frontier_->circuit_.get_Op_ptr_from_Vertex(v0);
    if (op->get_type() != OpType::Barrier) {
      int n_edges = this->mapping_frontier_->circuit_.n_in_edges_of_type(
          v0, EdgeType::Quantum);
      // make forwards = backwards
      if (n_edges == 2) {
        auto jt = it;
        ++jt;
        while (jt !=
               this->mapping_frontier_->linear_boundary->get<TagKey>().end()) {
          // i.e. if vertices match
          Edge e1 = this->mapping_frontier_->circuit_.get_nth_out_edge(
              jt->second.first, jt->second.second);
          Vertex v1 = this->mapping_frontier_->circuit_.target(e1);
          if (v0 == v1) {
            // we can assume a qubit will only be in one interaction
            // we can assume from how we iterate through pairs that each qubit
            // will only be found in one match
            bool node0_exists =
                this->architecture_->node_exists(Node(it->first));
            bool node1_exists =
                this->architecture_->node_exists(Node(jt->first));
            if (!node0_exists || !node1_exists || op->get_desc().is_box()) {
              all_placed = false;
              if (route_check == CheckRoutingValidity::Yes) return false;
            }

            if (assigned_only == AssignedOnly::No ||
                (node0_exists && node1_exists)) {
              interacting_uids_.insert({it->first, jt->first});
              interacting_uids_.insert({jt->first, it->first});
            }
          }
          ++jt;
        }
      }
    }
  }

  // conditions for proceeding with labelling
  if (label_check == CheckLabellingValidity::Yes) {
    if (all_placed) {
      return true;
    } else {
      return false;
    }
  }
  // this should have left early when first found
  if (route_check == CheckRoutingValidity::Yes) {
    if (all_placed && interacting_uids_.size() > 0) {
      return true;
    }
    return false;
  }
  // => either route_check true and all_placed so valid
  // or !route_check and !label_check so return true and discard
  return true;
}

swap_set_t LexiRoute::get_candidate_swaps() {
  swap_set_t candidate_swaps;
  for (const auto& interaction : this->interacting_uids_) {
    Node assigned_first = Node(this->labelling_[interaction.first]);
    std::vector<Node> adjacent_uids_0 =
        this->architecture_->nodes_at_distance(assigned_first, 1);
    TKET_ASSERT(adjacent_uids_0.size() != 0);
    for (const Node& neighbour : adjacent_uids_0) {
      if (candidate_swaps.find({neighbour, assigned_first}) ==
          candidate_swaps.end()) {
        candidate_swaps.insert({assigned_first, neighbour});
      }
    }
    Node assigned_second = Node(this->labelling_[interaction.second]);
    std::vector<Node> adjacent_uids_1 =
        this->architecture_->nodes_at_distance(assigned_second, 1);
    TKET_ASSERT(adjacent_uids_1.size() != 0);
    for (const Node& neighbour : adjacent_uids_1) {
      if (candidate_swaps.find({neighbour, assigned_second}) ==
          candidate_swaps.end()) {
        candidate_swaps.insert({assigned_second, neighbour});
      }
    }
  }
  return candidate_swaps;
}

bool is_vertex_CX(const Circuit& circ_, const Vertex& v) {
  Op_ptr op = circ_.get_Op_ptr_from_Vertex(v);
  while (op->get_type() == OpType::Conditional) {
    op = static_cast<const Conditional&>(*op).get_op();
  }
  return op->get_type() == OpType::CX;
}

std::pair<bool, bool> LexiRoute::check_bridge(
    const std::pair<Node, Node>& swap, unsigned lookahead) {
  std::pair<bool, bool> output = {false, false};
  // first confirm whether it even has an interaction
  auto it = this->interacting_uids_.find(swap.first);
  if (it != this->interacting_uids_.end()) {  // => in interaction
    if (this->architecture_->get_distance(swap.first, Node(it->second)) ==
        2) {  // => could be bridge
      // below should always return correct object given prior checks
      VertPort vp =
          (*this->mapping_frontier_->linear_boundary->find(swap.first)).second;
      Edge out_edge = this->mapping_frontier_->circuit_.get_nth_out_edge(
          vp.first, vp.second);
      output.first = is_vertex_CX(
          this->mapping_frontier_->circuit_,
          this->mapping_frontier_->circuit_.target(out_edge));
    }
  }
  // repeat for second swap
  it = this->interacting_uids_.find(swap.second);
  if (it != this->interacting_uids_.end()) {
    if (this->architecture_->get_distance(swap.second, Node(it->second)) == 2) {
      VertPort vp =
          (*this->mapping_frontier_->linear_boundary->find(swap.second)).second;
      Edge out_edge = this->mapping_frontier_->circuit_.get_nth_out_edge(
          vp.first, vp.second);
      output.second = is_vertex_CX(
          this->mapping_frontier_->circuit_,
          this->mapping_frontier_->circuit_.target(out_edge));
    }
  }
  if ((output.first && output.second) || (!output.first && !output.second)) {
    return {0, 0};
  }
  // implies conditions are set to at least check if BRIDGE is better
  swap_set_t candidate_swaps = {
      swap,
      {swap.first,
       swap.first}};  // second swap here will just compare the base case

  // as with best swap finder, we create a set of candidate swap gates and
  // then find best, except with only 2 swap (best swap and no swap)
  while (candidate_swaps.size() > 1 /*some lookahead parameter*/) {
    this->mapping_frontier_->advance_next_2qb_slice(lookahead);
    // true bool means it only sets interacting uids if both uids are in
    // architecture
    this->set_interacting_uids(
        AssignedOnly::Yes, CheckRoutingValidity::No,
        CheckLabellingValidity::No);
    // if 0, just take first swap rather than place
    if (this->interacting_uids_.size() == 0) {
      candidate_swaps = {*candidate_swaps.begin()};
    } else {
      interacting_nodes_t convert_uids;
      for (const auto& p : this->interacting_uids_) {
        convert_uids.insert(
            {Node(this->labelling_[p.first]),
             Node(this->labelling_[p.second])});
      }
      LexicographicalComparison lookahead_lc(this->architecture_, convert_uids);
      lookahead_lc.remove_swaps_lexicographical(candidate_swaps);
    }
  }
  // condition implies bridge is chosen
  // if both remained then lexicographically equivalent under given conditions
  // so either can be added with same consequences (for given hyper
  // parameters)
  if (*candidate_swaps.begin() == swap) {
    output = {0, 0};
  }
  return output;
}

// Returns the distance between n1 and p1 and the distance between n2 and p2,
// distance ordered (greatest first)
const std::pair<std::size_t, std::size_t> LexiRoute::pair_distances(
    const Node& p0_first, const Node& p0_second, const Node& p1_first,
    const Node& p1_second) const {
  {
    const bool valid = this->architecture_->node_exists(p0_first) &&
                       this->architecture_->node_exists(p0_second) &&
                       this->architecture_->node_exists(p1_first) &&
                       this->architecture_->node_exists(p1_second);
    TKET_ASSERT(valid);
  }
  std::size_t curr_dist1 =
      this->architecture_->get_distance(p0_first, p0_second);
  std::size_t curr_dist2 =
      this->architecture_->get_distance(p1_first, p1_second);
  return (curr_dist1 > curr_dist2) ? std::make_pair(curr_dist1, curr_dist2)
                                   : std::make_pair(curr_dist2, curr_dist1);
}

void LexiRoute::remove_swaps_decreasing(swap_set_t& swaps) {
  swap_set_t remaining_swaps;
  Node pair_first, pair_second;
  for (const auto& swap : swaps) {
    auto it = this->interacting_uids_.find(swap.first);
    // => swap.first is in interaction
    if (it != this->interacting_uids_.end()) {
      // find its pair
      pair_first = Node(it->second);
    } else {
      // => not interacting, assign pair to self (will give lexicographic
      // distance 0)
      pair_first = swap.first;
    }
    // => UnitID in SWAP are interacting
    if (pair_first == swap.second) {
      continue;
    }
    auto jt = this->interacting_uids_.find(swap.second);
    // => swap.second is in interaction
    if (jt != this->interacting_uids_.end()) {
      pair_second = Node(jt->second);
    } else {
      pair_second = swap.second;
    }
    // => UnitID in SWAP are interacting
    // Check should already be done with earlier continue
    TKET_ASSERT(pair_second != swap.first);

    const std::pair<std::size_t, std::size_t>& curr_dists =
        this->pair_distances(swap.first, pair_first, swap.second, pair_second);
    const std::pair<std::size_t, std::size_t>& news_dists =
        this->pair_distances(swap.second, pair_first, swap.first, pair_second);
    if (news_dists >= curr_dists) {
      continue;
    }
    remaining_swaps.insert(swap);
  }
}

bool LexiRoute::solve_labelling() {
  bool all_labelled = this->set_interacting_uids(
      AssignedOnly::No, CheckRoutingValidity::No, CheckLabellingValidity::Yes);
  if (!all_labelled) {
    // Boxes can block the frontier even when all their qubits are placed.
    // Let later routing methods run if labelling does not change anything.
    return this->update_labelling();
  }
  return false;
}

bool LexiRoute::solve(unsigned lookahead) {
  // work out if valid

  bool all_labelled = this->set_interacting_uids(
      AssignedOnly::No, CheckRoutingValidity::Yes, CheckLabellingValidity::No);
  if (!all_labelled) {
    return false;
  }

  // store a copy of the original this->mapping_frontier_->quantum_boundray
  // this object will be updated and reset throughout the swap picking
  // procedure so need to return it to original setting at end
  unit_vertport_frontier_t copy;
  for (const std::pair<UnitID, VertPort>& pair :
       this->mapping_frontier_->linear_boundary->get<TagKey>()) {
    copy.insert({pair.first, pair.second});
  }
  swap_set_t candidate_swaps = this->get_candidate_swaps();
  this->remove_swaps_decreasing(candidate_swaps);
  TKET_ASSERT(candidate_swaps.size() != 0);
  // Only want to substitute a single swap
  // check next layer of interacting qubits and remove swaps until only one
  // lexicographically superior swap is left
  unsigned counter = 0;
  while (candidate_swaps.size() > 1 && counter < lookahead) {
    // if 0, just take first swap rather than place
    if (this->interacting_uids_.size() == 0) {
      break;
    } else {
      interacting_nodes_t convert_uids;
      for (const auto& p : this->interacting_uids_) {
        convert_uids.insert(
            {Node(this->labelling_[p.first]),
             Node(this->labelling_[p.second])});
      }
      LexicographicalComparison lookahead_lc(this->architecture_, convert_uids);
      lookahead_lc.remove_swaps_lexicographical(candidate_swaps);
    }
    counter++;
    this->mapping_frontier_->advance_next_2qb_slice(lookahead);
    // true bool means it only sets interacting uids if both uids are in
    // architecture
    this->set_interacting_uids(
        AssignedOnly::Yes, CheckRoutingValidity::No,
        CheckLabellingValidity::No);
  }
  // find best swap
  auto it = candidate_swaps.end();
  --it;

  std::pair<Node, Node> chosen_swap = *it;
  this->mapping_frontier_->set_linear_boundary(copy);

  this->set_interacting_uids(
      AssignedOnly::No, CheckRoutingValidity::No, CheckLabellingValidity::No);
  std::pair<bool, bool> check = this->check_bridge(chosen_swap, lookahead);
  // set for final time, to allow gates to be correctly inserted, but then
  // leave as is insert gates
  this->mapping_frontier_->set_linear_boundary(copy);
  if (!check.first && !check.second) {
    // update circuit with new swap
    // final_labelling is initial labelling permuted by single swap

    // if false, SWAP is identical to last SWAP added without any gates
    // realised
    if (!this->mapping_frontier_->add_swap(
            chosen_swap.first, chosen_swap.second)) {
      // only need to reset in bridge case
      this->set_interacting_uids(
          AssignedOnly::No, CheckRoutingValidity::No,
          CheckLabellingValidity::No);
      // if SWAP has both Nodes in interaction default takes first
      // this could be inefficient compared to other SWAP, however
      // this failsafe is expected to be called extremely rarely (once in
      // thousands) on very dense circuits for very symmetrical architectures
      // so doesn't largely matter
      auto it = this->interacting_uids_.find(chosen_swap.first);
      auto add_path_swaps = [this](const Node& source, const Node& target) {
        auto path = this->architecture_->get_path(source, target);
        auto path_it_0 = path.begin() + 1;
        auto path_it_1 = path.begin();
        // adds a SWAP between each pair of adjacent nodes on path
        while (path_it_0 != path.end() - 1) {
          this->mapping_frontier_->add_swap(*path_it_0, *path_it_1);
          ++path_it_0;
          ++path_it_1;
        }
      };
      if (it != this->interacting_uids_.end()) {
        add_path_swaps(chosen_swap.first, Node(it->second));
      } else {
        it = this->interacting_uids_.find(chosen_swap.second);
        TKET_ASSERT(it != this->interacting_uids_.end());
        add_path_swaps(chosen_swap.second, Node(it->second));
      }
    }

  } else {
    // only need to reset in bridge case
    this->set_interacting_uids(
        AssignedOnly::No, CheckRoutingValidity::No, CheckLabellingValidity::No);

    auto add_ordered_bridge = [&](const Node& n) {
      auto it0 = this->mapping_frontier_->linear_boundary->find(n);
      // this should implicitly be the case if this logic is reached
      TKET_ASSERT(it0 != this->mapping_frontier_->linear_boundary->end());

      Node other_node = Node(this->interacting_uids_[n]);
      auto it1 = this->mapping_frontier_->linear_boundary->find(other_node);
      // this should implicitly be the case if this logic is reached
      TKET_ASSERT(it1 != this->mapping_frontier_->linear_boundary->end());

      auto path = this->architecture_->get_path(n, other_node);
      Node central = Node(path[1]);

      Edge n_edge = this->mapping_frontier_->circuit_.get_nth_out_edge(
          it0->second.first, it0->second.second);
      Edge other_edge = this->mapping_frontier_->circuit_.get_nth_out_edge(
          it1->second.first, it1->second.second);

      unsigned port0 =
          this->mapping_frontier_->circuit_.get_target_port(n_edge);
      unsigned port1 =
          this->mapping_frontier_->circuit_.get_target_port(other_edge);
      // compare port ordering to get control vs target
      TKET_ASSERT(port0 != port1);
      if (port0 < port1) {
        this->mapping_frontier_->add_bridge(n, central, other_node);
      } else {
        this->mapping_frontier_->add_bridge(other_node, central, n);
      }
    };

    if (check.first) {
      add_ordered_bridge(chosen_swap.first);
    }
    if (check.second) {
      add_ordered_bridge(chosen_swap.second);
    }
  }
  return true;
}

}  // namespace tket
