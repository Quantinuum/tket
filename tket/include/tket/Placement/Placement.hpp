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

#include "tket/Architecture/Architecture.hpp"
#include "tket/Characterisation/DeviceCharacterisation.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Placement/QubitGraph.hpp"

#include "tket_export.h"

namespace tket {

class TKET_EXPORT Placement {
 public:
  typedef std::shared_ptr<Placement> Ptr;

  explicit Placement(const Architecture& _architecture);

  Placement() {};

  /**
   * Reassigns some UnitID in circ_ as UnitID in architecture_
   *
   * @param circ_ Circuit to be relabelled
   * @param compilation_map For tracking changes during compilation
   *
   * @return true iff circuit or maps are modified
   */
  bool place(
      Circuit& circ_,
      std::shared_ptr<unit_bimaps_t> compilation_map = nullptr) const;

  /**
   * Reassigns some UnitID in circ_ as UnitID in architecture_, according to
   * given map.
   *
   * @param circ Circuit to be relabelled
   * @param map_ relabelling
   * @param compilation_map For tracking changes during compilation
   *
   * @return true iff circuit or maps were modified
   */
  static bool place_with_map(
      Circuit& circ, std::map<Qubit, Node>& map_,
      std::shared_ptr<unit_bimaps_t> compilation_map = nullptr);

  /**
   *
   * For some Circuit, returns a map between Circuit UnitID and
   * Architecture UnitID that can be used for reassigning UnitID in
   * Circuit. Map is expected to give best performance for given method.
   *
   * @param circ_ Circuit relabelling map is constructed from
   *
   * @return Map between Circuit and Architecture UnitID
   */
  std::map<Qubit, Node> get_placement_map(const Circuit& circ_) const;

  /**
   *
   * For some Circuit, returns maps between Circuit UnitID and
   * Architecture UnitID that can be used for reassigning UnitID in
   * Circuit. Maps expected to give similar performance for given method.
   * For Placement this naively assigns every Qubit to some Node.
   *
   * @param circ_ Circuit relabelling map is constructed from
   *
   * @return Map between Circuit and Architecture UnitID
   */
  virtual std::vector<std::map<Qubit, Node>> get_all_placement_maps(
      const Circuit& circ_, unsigned /*matches*/) const;

  /**
   * Returns a reference to held Architecture.
   * Used to know Architecture properties to set predicates
   * during compilation.
   *
   * @return Architecture
   */
  const Architecture& get_architecture_ref() { return architecture_; }

  virtual ~Placement() {};

  static const std::string& unplaced_reg();

 protected:
  Architecture architecture_;
};

JSON_DECL(Placement::Ptr);

class TKET_EXPORT GraphPlacement : public Placement {
 public:
  /**
   * Holds information for constructing a weighted edge in a QubitGraph.
   * @param node0 UnitID for first node in edge
   * @param node1 UnitID for second node in edge
   * @param weight Unsigned giving a weight for implied edge
   * @param distance Distance between Node on some graph
   */
  struct WeightedEdge {
    UnitID node0;
    UnitID node1;
    unsigned weight;
    unsigned distance;
  };

  /**
   * Holds information for slice wise iteration of Circuit
   * @param _circ Circuit to iterate through
   */
  struct Frontier {
    // set of 2qb vertices which need to be solved for
    std::shared_ptr<Slice> slice;
    // Quantum Edges coming in to vertices in slice, indexed by qubit
    std::shared_ptr<unit_frontier_t> quantum_in_edges;
    // Quantum Edges leaving vertices in slice, indexed by qubit
    std::shared_ptr<unit_frontier_t> quantum_out_edges;
    // Boolean edges coming in to vertices in slice. Guarantees that all edges
    // into every vertex in slice is represented in next_cut
    std::shared_ptr<b_frontier_t> boolean_in_edges;

    // reference to circuit that it acts on
    const Circuit& circ;

    // initialise at front of circuit
    explicit Frontier(const Circuit& _circ);
    // move to next slice
    void next_slicefrontier();
  };

  explicit GraphPlacement(
      const Architecture& _architecture, unsigned maximum_matches = 2000,
      unsigned timeout = 100, unsigned maximum_pattern_gates = 100,
      unsigned maximum_pattern_depth = 100);
  /**
   * For some Circuit, returns maps between Circuit UnitID and
   * Architecture UnitID that can be used for reassigning UnitID in
   * Circuit. Maps are constructed by running a Weighted Subgraph Monomorphism
   * for the given problem and returning up to matches number of
   * potential solutions, ranked.
   *
   * @param circ_ Circuit relabelling map is constructed from
   * @param matches Maximum number of matches found during WSM.
   * @return Map between Circuit and Architecture UnitID
   */
  std::vector<std::map<Qubit, Node>> get_all_placement_maps(
      const Circuit& circ_, unsigned matches) const override;

  /**
   * @return maximum matches found during placement
   */
  unsigned get_maximum_matches() const { return this->maximum_matches_; }

  /**
   * @return maximum time (ms)
   */
  unsigned get_timeout() const { return this->timeout_; }

  /**
   * @return maximum gates to construct pattern graph from
   */
  unsigned get_maximum_pattern_gates() const {
    return this->maximum_pattern_gates_;
  }

  /**
   * @return maximum depth to search to find gates to construct pattern graph
   * from
   */
  unsigned get_maximum_pattern_depth() const {
    return this->maximum_pattern_depth_;
  }

 protected:
  unsigned maximum_matches_;
  unsigned timeout_;
  unsigned maximum_pattern_gates_;
  unsigned maximum_pattern_depth_;

  mutable std::vector<WeightedEdge> weighted_target_edges;

  //   we can use a vector as we index by incrementing size
  mutable std::vector<Architecture::UndirectedConnGraph> extended_target_graphs;

  const std::vector<WeightedEdge> default_pattern_weighting(
      const Circuit& circuit) const;
  const std::vector<WeightedEdge> default_target_weighting(
      Architecture& passed_architecture) const;

  QubitGraph construct_pattern_graph(
      const std::vector<WeightedEdge>& edges, unsigned max_out_degree) const;

  Architecture construct_target_graph(
      const std::vector<WeightedEdge>& edges, unsigned distance) const;

  std::vector<boost::bimap<Qubit, Node>>
  get_all_weighted_subgraph_monomorphisms(
      const Circuit& circ_,
      const std::vector<WeightedEdge>& weighted_pattern_edges,
      bool return_best) const;

  std::map<Qubit, Node> convert_bimap(
      boost::bimap<Qubit, Node>& bimap,
      const QubitGraph::UndirectedConnGraph& pattern_graph) const;
};

/** Solves the pure unweighted subgraph monomorphism problem, trying
 * to embed the pattern graph into the target graph.
 * Note that graph edge weights are IGNORED by this function.
 */
std::vector<boost::bimap<Qubit, Node>> get_weighted_subgraph_monomorphisms(
    QubitGraph::UndirectedConnGraph& pattern_graph,
    Architecture::UndirectedConnGraph& target_graph, unsigned max_matches,
    unsigned timeout_ms, bool return_best);

class TKET_EXPORT LinePlacement : public GraphPlacement {
 public:
  explicit LinePlacement(
      const Architecture& _architecture, unsigned _maximum_pattern_gates = 100,
      unsigned _maximum_pattern_depth = 100);
  /**
   * For some Circuit, returns maps between Circuit UnitID and
   * Architecture UnitID that can be used for reassigning UnitID in
   * Circuit. Maps are constructed by converting qubit interactions
   * into a sequence of lines and assigning them to a
   * Hamiltonian path of the target graph.
   *
   * @param circ_ Circuit relabelling map is constructed from
   * @return Map between Circuit and Architecture UnitID
   */
  std::vector<std::map<Qubit, Node>> get_all_placement_maps(
      const Circuit& circ_, unsigned /*matches*/) const override;

 private:
  std::vector<qubit_vector_t> interactions_to_lines(const Circuit& circ_) const;

  std::map<Qubit, Node> assign_lines_to_target_graph(
      std::vector<qubit_vector_t>& line_pattern, unsigned n_qubits) const;
};

class TKET_EXPORT NoiseAwarePlacement : public GraphPlacement {
 public:
  explicit NoiseAwarePlacement(
      const Architecture& _architecture,
      std::optional<avg_node_errors_t> _node_errors = std::nullopt,
      std::optional<avg_link_errors_t> _link_errors = std::nullopt,
      std::optional<avg_readout_errors_t> _readout_errors = std::nullopt,
      unsigned _maximum_matches = 2000, unsigned _timeout = 100,
      unsigned _maximum_pattern_gates = 100,
      unsigned _maximum_pattern_depth = 100);

  /**
   * For some Circuit, returns maps between Circuit UnitID and
   * Architecture UnitID that can be used for reassigning UnitID in
   * Circuit. Maps are constructed by running a Weighted Subgraph Monomorphism
   * for the given problem and returning up to matches number of
   * potential solutions, ranked. Additionally, the top
   * x mappings with identical WSM score is costed
   * depending on passed Device characteristics, effecting
   * the ranking.
   *
   * @param circ_ Circuit relabelling map is constructed from
   * @param matches Maximum number of matches found during WSM.
   * @return Map between Circuit and Architecture UnitID
   */
  std::vector<std::map<Qubit, Node>> get_all_placement_maps(
      const Circuit& circ_, unsigned matches) const override;

  /**
   * @return A DeviceCharacterisation object storing Architecture errors
   */
  DeviceCharacterisation get_characterisation() const;

  /**
   * @param characterisation Error information for Architecture
   */
  void set_characterisation(const DeviceCharacterisation& characterisation);

 private:
  DeviceCharacterisation characterisation_;

  std::vector<boost::bimap<Qubit, Node>> rank_maps(
      const std::vector<boost::bimap<Qubit, Node>>& placement_maps,
      const Circuit& circ_,
      const std::vector<WeightedEdge>& pattern_edges) const;
  double cost_placement(
      const boost::bimap<Qubit, Node>& map, const Circuit& circ_,
      const QubitGraph& q_graph) const;
};

void to_json(nlohmann::json& j, const Placement::Ptr& placement_ptr);
void from_json(const nlohmann::json& j, Placement::Ptr& placement_ptr);

}  // namespace tket
