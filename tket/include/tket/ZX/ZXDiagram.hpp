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

#include "tket/ZX/ZXDiagramImpl.hpp"

#include "tket_export.h"

namespace tket {

namespace zx {

// Forward declare Rewrite, ZXDiagramPybind, Flow for friend access
class Rewrite;
class ZXDiagramPybind;
class Flow;

class TKET_EXPORT ZXDiagram {
 private:
  /**
   * Underlying representation
   */

  // Underlying graph
  std::unique_ptr<ZXGraph> graph;

  /**
   * Boundary vertices in addressable order.
   * This may include both Quantum and Classical boundaries.
   * Each boundary vertex can be an Input, Output, or Open (for generic
   * undirected boundary points).
   */
  ZXVertVec boundary;

  // Global scalar for tracking during rewrites
  Expr scalar;

 public:
  /**
   * Constructors & assignment operators for:
   * - empty diagram,
   * - empty diagram with specific input / output signatures
   * - copy constructor
   * - copy assignment
   */
  ZXDiagram();
  ZXDiagram(
      unsigned in, unsigned out, unsigned classical_in, unsigned classical_out);
  ZXDiagram(const ZXDiagram& other);
  ZXDiagram(ZXDiagram&& other);
  ZXDiagram& operator=(const ZXDiagram& other);
  ZXDiagram& operator=(ZXDiagram&& other);

  /**
   * Getters & Setters
   */
  // If `qtype` is given, then a copy of the boundary subset will be returned
  ZXVertVec get_boundary(
      std::optional<ZXType> type = std::nullopt,
      std::optional<QuantumType> qtype = std::nullopt) const;

  // TODO discuss whether to expose these two
  std::unique_ptr<ZXGraph>& get_graph();
  void add_boundary(ZXVert& vert);

  // Getting the global scalar and modifying by multiplication
  const Expr& get_scalar() const;
  void multiply_scalar(const Expr& sc);

  // Counting all vertices / wires in the diagram
  unsigned n_vertices() const;
  unsigned n_wires() const;

  // Count number of vertices with certain types & properties
  unsigned count_vertices(ZXType type) const;
  unsigned count_vertices(ZXType zxtype, QuantumType qtype) const;
  unsigned count_wires(ZXWireType type) const;

  // Local properties on vertices and edges
  unsigned degree(const ZXVert& v) const;
  // Neighbours given in order of iteration through the underlying boost graph,
  // which is deterministic from a given insertion order but need not be
  // semantically relevant
  ZXVertVec neighbours(const ZXVert& v) const;
  // Incident wires given in order of iteration through the underlying boost
  // graph, outedges (and self-loops) before inedges (ignoring self-loops)
  WireVec adj_wires(const ZXVert& v) const;
  // Wires given in the same order as `adj_wires(u)`
  WireVec wires_between(const ZXVert& u, const ZXVert& v) const;

  /**
   * Searches for an arbitrary wire between `va` and `vb`.
   * If none exists, then `std::nullopt` is returned.
   * `directed` controls the search for semantically undirected edges within
   * the underlying directed graph structure and should be called with
   * `UNDIRECTED` unless there is good reason to not.
   */
  enum class WireSearchOption { UNDIRECTED, DIRECTED };
  std::optional<Wire> wire_between(
      const ZXVert& va, const ZXVert& vb,
      WireSearchOption directed = WireSearchOption::UNDIRECTED) const;

  Wire wire_at_port(const ZXVert& v, std::optional<unsigned> port) const;

  // Getting/setting vertex properties
  ZXGen_ptr get_vertex_ZXGen_ptr(const ZXVert& v) const;
  template <
      typename T, typename = typename std::enable_if<
                      std::is_base_of<ZXGen, T>::value, bool>::type>
  const T& get_vertex_ZXGen(const ZXVert& v) const;
  std::string get_name(const ZXVert& v) const;
  ZXType get_zxtype(const ZXVert& v) const;
  std::optional<QuantumType> get_qtype(const ZXVert& v) const;
  void set_vertex_ZXGen_ptr(const ZXVert& v, const ZXGen_ptr& op);

  // Getting/setting wire properties
  WireProperties get_wire_info(const Wire& w) const;
  QuantumType get_qtype(const Wire& w) const;
  ZXWireType get_wire_type(const Wire& w) const;
  ZXVert source(const Wire& w) const;
  ZXVert target(const Wire& w) const;
  std::optional<unsigned> source_port(const Wire& w) const;
  std::optional<unsigned> target_port(const Wire& w) const;
  // Returns the vertex on `w` at the other end from `u`
  ZXVert other_end(const Wire& w, const ZXVert& u) const;
  ZXVert vertex_at_end(const Wire& w, WireEnd we) const;
  // Returns which end of `w` is connected to `u`
  WireEnd end_of(const Wire& w, const ZXVert& u) const;
  void set_wire_info(const Wire& w, const WireProperties& wp);
  void set_wire_qtype(const Wire& w, QuantumType qtype);
  void set_wire_type(const Wire& w, ZXWireType type);

  /**
   * Shortcut utilities to detect whether `v` is a spider with a Pauli /
   * proper Clifford phase
   */
  bool is_pauli_spider(const ZXVert& v) const;
  bool is_proper_clifford_spider(const ZXVert& v) const;

  /**
   * Check for well-formedness of the internal graph.
   * Checks:
   * - Inputs/Outputs have degree 1 and all exist within the boundary.
   * - Undirected vertices have no ports on incident edges.
   * - Directed vertices have exactly one incident edge for each port.
   * - All incident edges to vertex have a QuantumType compatible with that
   * port. Throws an exception if one of the checks fail. Time complexity: O(V +
   * E), for a graph with bounded degrees.
   */
  void check_validity() const;

  /**
   * Symbolic manipulation
   */
  void symbol_substitution(const symbol_map_t& symbol_map);
  void symbol_substitution(
      const std::map<Sym, double, SymEngine::RCPBasicKeyLess>& symbol_map);
  void symbol_substitution(const SymEngine::map_basic_basic& sub_map);

  // Set of all free symbols occurring in operation parameters
  SymSet free_symbols() const;

  // Whether the diagram contains any symbolic parameters
  bool is_symbolic() const;

  // Whether the diagram is graphlike (ZSpiders and H edges, Basics to
  // boundaries)
  bool is_graphlike() const;

  // Whether the diagram is MBQC (MBQC, Inputs, and Outputs, Basic to
  // boundaries, H otherwise)
  bool is_MBQC() const;

  /**
   * Produces graphviz string, applying `highlights` to some vertices.
   * Inputs:
   *  highlights: set of vertices to highlight (with a red star) in the
   *    visualisation. These should be valid vertices on `graph`.
   **/
  std::string to_graphviz_str(const std::set<ZXVert>& highlights = {}) const;

  /**
   * Diagram manipulation
   */
  /**
   * Adds a vertex with a given generator to the diagram.
   * It is initially disconnected from any other vertex.
   * Automatically adds boundary generators to the boundary.
   */
  ZXVert add_vertex(ZXGen_ptr op);
  ZXVert add_vertex(ZXType type, QuantumType qtype = QuantumType::Quantum);
  ZXVert add_vertex(
      ZXType type, const Expr& param, QuantumType qtype = QuantumType::Quantum);
  ZXVert add_clifford_vertex(
      ZXType type, bool param, QuantumType qtype = QuantumType::Quantum);

  /**
   * Adds a wire between `va` and `vb` by a `WireProperties` object.
   * Uses `va` as the source and `vb` as the target in the underlying directed
   * graph structure.
   **/
  Wire add_wire(const ZXVert& va, const ZXVert& vb, const WireProperties& prop);
  /**
   * Adds a wire of specific types between `va` -> `vb`.
   * `va_port`, `vb_port` are used by `DirectedZXGenerator` objects
   * to specify which port on `va`, `vb` (respectively) the wire plugs into.
   *
   * See `ZXGenerator.hpp` --> `DirectedZXGenerator` for more
   * information, including the definition of the port numbers.
   * Returns:
   *  The successfully added `Wire`.
   **/
  Wire add_wire(
      const ZXVert& va, const ZXVert& vb, ZXWireType type = ZXWireType::Basic,
      QuantumType qtype = QuantumType::Quantum,
      std::optional<unsigned> va_port = std::nullopt,
      std::optional<unsigned> vb_port = std::nullopt);

  /**
   * Removes the vertex `v`, along with all wires connected to it.
   * Boundary updated if vertex is in the `boundary`.
   **/
  void remove_vertex(const ZXVert& v);
  void remove_wire(const Wire& w);

  /**
   * Removes exactly 1 wire that has property `prop` from vertex `va` -> `vb`.
   * If there exist multiple edges matching the property, they are
   * indistinguishable, so we just remove one arbitrarily. In this case, the
   * only difference is the edge descriptor. If the user cares about
   * preserving particular edge descriptors, remove_wire(const Wire&) should
   * be used.
   * Inputs:
   *  va, vb: (ordered) vertices to remove possible wire from
   *  prop: properties to look for in the wires between the two vertices
   *  directed: whether to try also with `vb` -> `va` (i.e. when we are
   *    trying to remove edges of unknown direction)
   * Return value indicates whether an edge was removed.
   **/
  bool remove_wire(
      const ZXVert& va, const ZXVert& vb, const WireProperties& prop,
      WireSearchOption directed = WireSearchOption::UNDIRECTED);

  /**
   * Diagram conversion
   */

  /**
   * Expands quantum vertices into pairs of classical vertices according to
   * the "doubling" construction for CPM.
   * New boundary vertices are ordered lexicographically by (b, c):
   * - b boundary index in original diagram
   * - c conjugate identifier
   *  + quantum boundaries are mapped to a pair with original and conjugated
   *      phases
   *  + unconjugated first
   *  + classical boundaries only have the unconjugated version
   */
  ZXDiagram to_doubled_diagram() const;

  /**
   * Expands classical boundary vertices into quantum boundaries via input
   * extension with a ZSpider.
   * Effectively prepends Z basis measurements and appends Z basis
   * initialisations for each classical bit.
   * This is used to map the entire semantics into a single CPTP-map, which
   * can be represented by its Choi matrix.
   */
  ZXDiagram to_quantum_embedding() const;

  /**
   * Subdiagram
   *
   * Represents a closed region of the diagram by taking a cut through a set
   * of edges. If the subdiagram were treated as a new ZXDiagram object:
   * - The boundary order is as given by `boundary`.
   * - All boundary vertices are treated as Open.
   * - Boundary vertices inherit their QuantumType from the original Wire.
   * - Boundary edges are all Basic (i.e. the Hadamard from Hadamard
   *      wires in the `boundary` list are treated as outside of the
   *      Subdiagram).
   * Each wire in the boundary is tagged with the WireEnd facing the interior of
   * the subdiagram. If a wire appears with both ends, they are treated as two
   * separate boundary wires split by an identity (Basic) or Hadamard.
   */
  struct Subdiagram {
    // Ordered boundary edges of the subdiagram
    std::vector<std::pair<Wire, WireEnd>> boundary_;
    // All vertices within the subdiagram
    ZXVertSeqSet verts_;

    Subdiagram();
    Subdiagram(
        const std::vector<std::pair<Wire, WireEnd>>& cut,
        const ZXVertSeqSet& verts);

    /**
     * Checks for well-formedness of a subdiagram.
     * - For each wire in `boundary`, exactly one end of it is in `verts`.
     * - For each vertex in `verts`, every incident edge is either in `boundary`
     * or connects to another vertex in `verts`.
     * - There are no boundary vertices in `verts`.
     * Throws an exception if any of these fail.
     */
    void check_validity(const ZXDiagram& diag) const;

    // Copy the Subdiagram into a new ZXDiagram object.
    ZXDiagram to_diagram(const ZXDiagram& orig) const;
  };

  void substitute(const ZXDiagram& to_insert, const Subdiagram& to_replace);

  friend Rewrite;
  friend ZXDiagramPybind;
  friend Flow;

 private:
  /**
   * Copy the `other`'s graph over into this diagram (such that `other.graph`)
   * is a subgraph of `this`'s graph.
   * Inputs:
   *  @param other the diagram to copy into `this` one
   *  @param merge_boundaries whether to update the boundary information on
   *    `this` diagram, combining the two diagrams' boundaries together. The
   *    newly added boundary elements will preserve their original order in
   *    `other`, and appended to the end of `this`'s boundary vector.
   * Returns the isomorphism from the `other`'s graph into its copy in
   * `this`'s graph.
   *
   * Users include:
   *  - the copy assignment `operator=`,
   *  - the copy constructor
   **/
  std::pair<std::map<ZXVert, ZXVert>, std::map<Wire, Wire>> copy_graph(
      const ZXDiagram& other, bool merge_boundaries = true);
};

template <typename T, typename>
const T& ZXDiagram::get_vertex_ZXGen(const ZXVert& v) const {
  ZXGen_ptr pt = get_vertex_ZXGen_ptr(v);
  return dynamic_cast<const T&>(*pt);
}

}  // namespace zx

}  // namespace tket
