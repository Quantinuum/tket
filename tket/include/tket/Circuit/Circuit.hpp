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

// NOTE: FOR ALL COMMENTS ON SCALING 'alpha' IS THE MAXIMUM ARITY OF VERTICES IN
// THE GRAPH CIRCUITS ARE TYPICALLY SPARSE UNLESS THEY CONTAIN LARGE PHASE
// GADGETS/BOXES CURRENTLY ie. `alpha` ~= 1-3 typically Other common variables:
// D - depth of circuit
// V - no. vertices in circuit
// E - no. edges in circuit
// q - no. qubits
// Other notes: the worstcase possibility of hashtable lookup (linear)
// is ignored, and the amortized constant time used for scaling instead

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <tkassert/Assert.hpp>
#include <tklog/TketLog.hpp>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Boxes.hpp"
#include "Command.hpp"
#include "Conditional.hpp"
#include "DAGDefs.hpp"
#include "ResourceData.hpp"
#include "Slices.hpp"
#include "tket/Gate/GatePtr.hpp"
#include "tket/Gate/OpPtrFunctions.hpp"
#include "tket/OpType/EdgeType.hpp"
#include "tket/Utils/Json.hpp"
#include "tket/Utils/SequencedContainers.hpp"
#include "tket/Utils/UnitID.hpp"

#include "tket_export.h"

namespace tket {

typedef std::vector<EdgeVec> BundleVec;

typedef std::vector<Slice> SliceVec;

typedef std::vector<VertPort> QPathDetailed;
typedef std::unordered_map<Vertex, Vertex> vertex_map_t;

/* these are used only for pattern matching */
typedef std::map<Edge, Edge> edge_map_t;

struct BoundaryElement {
  UnitID id_;
  Vertex in_;
  Vertex out_;
  UnitType type() const { return id_.type(); }
  std::string reg_name() const { return id_.reg_name(); }
  register_info_t reg_info() const { return id_.reg_info(); }

  bool operator==(const BoundaryElement &other) const {
    return this->id_ == other.id_ && this->in_ == other.in_ &&
           this->out_ == other.out_;
  }
};

struct TagID {};
struct TagIn {};
struct TagOut {};
struct TagType {};
struct TagReg {};
typedef boost::multi_index::multi_index_container<
    BoundaryElement,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
            boost::multi_index::tag<TagID>,
            boost::multi_index::member<
                BoundaryElement, UnitID, &BoundaryElement::id_>>,
        boost::multi_index::ordered_unique<
            boost::multi_index::tag<TagIn>,
            boost::multi_index::member<
                BoundaryElement, Vertex, &BoundaryElement::in_>>,
        boost::multi_index::ordered_unique<
            boost::multi_index::tag<TagOut>,
            boost::multi_index::member<
                BoundaryElement, Vertex, &BoundaryElement::out_>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<TagType>,
            boost::multi_index::const_mem_fun<
                BoundaryElement, UnitType, &BoundaryElement::type>>,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<TagReg>,
            boost::multi_index::const_mem_fun<
                BoundaryElement, std::string, &BoundaryElement::reg_name>>>>
    boundary_t;

typedef std::unordered_map<unsigned, unsigned> permutation_t;

/**
 * Structure to describe a convex region of the interaction graph.
 * Usually used to identify a region to replace by another circuit.
 *
 * The Subcircuit is valid if there exist two possible frontiers (complete cuts)
 * through the circuit such that:
 * - in_hole is contained within the first frontier
 * - out_hole consists of exactly the edges in the later frontier corresponding
 *      to the same units as those in in_hole
 * - b_future corresponds exactly to the set of Boolean edges in the
 *      later frontier whose origins are in out_hole (these are needed when
 *      an edge is in both in_hole and out_hole to determine where the
 *      replacement circuit should be placed with respect to the vertices with
 *      Booleans from that unit)
 */
struct Subcircuit {
  /** Ordered incoming edges into the subcircuit */
  EdgeVec in_hole;
  /** Ordered linear edges out of the subcircuit */
  std::vector<std::optional<Edge>> out_hole;
  /** Boolean edges in the future of the subcircuit, to be rewired to
   * the replacement of the corresponding edge from out_hole
   */
  EdgeVec b_future;
  VertexSet verts;

  Subcircuit() {}
  Subcircuit(
      const EdgeVec &ins, const std::vector<std::optional<Edge>> &outs,
      const EdgeVec &b_outs, const VertexSet &vs)
      : in_hole(ins), out_hole(outs), b_future(b_outs), verts(vs) {}
};

// Used for edge traversal in pattern-matching
struct TraversalPoint {
  Vertex from;
  Vertex to;
  Edge edge;
};

// list of error types to throw out
class CircuitInequality : public std::logic_error {
 public:
  explicit CircuitInequality(const std::string &message)
      : std::logic_error(message) {}
};
class CircuitInvalidity : public std::logic_error {
 public:
  explicit CircuitInvalidity(const std::string &message)
      : std::logic_error(message) {}
};

class Unsupported : public std::logic_error {
 public:
  explicit Unsupported(const std::string &message)
      : std::logic_error(message) {}
};

class MissingVertex : public std::logic_error {  // Necessary?
  // Q: boost might pick this up and we can just catch it?
  // A : it does not
 public:
  MissingVertex() : std::logic_error("unknown vertex missing") {}
  explicit MissingVertex(const std::string &error_string)
      : std::logic_error(error_string) {}
};

class MissingEdge : public std::logic_error {
 public:
  explicit MissingEdge(const tket::Edge &edge)
      : std::logic_error("Edge missing") {
    std::stringstream s;
    s << edge;
    tket_log()->info(s.str());
  }
  MissingEdge() : std::logic_error("unknown edge missing") {}
};

class SimpleOnly : public Unsupported {
 public:
  SimpleOnly()
      : Unsupported(
            "Function only allowed for simple circuits (single "
            "register)") {}
};

class CompilationUnit;

enum ReverseType {
  dagger = 1,
  transpose = 2,
};

/**
 * A circuit.
 *
 * A circuit comprises some quantum and classical wires and a defined sequence
 * of operations on them with a defined global phase.
 */
class TKET_EXPORT Circuit {
  void _handle_boundaries(Circuit &circ, vertex_map_t &vmap) const;
  void _handle_interior(
      Circuit &circ, vertex_map_t &vmap, V_iterator &vi, V_iterator &vend,
      ReverseType reverse_op) const;
  void _handle_edges(
      Circuit &circ, vertex_map_t &vmap, E_iterator &ei,
      E_iterator &eend) const;

 public:
  SliceIterator slice_begin() const;
  static SliceIterator slice_end();
  static const SliceIterator nullsit;

  unit_vector_t args_from_frontier(
      const Vertex &vert, std::shared_ptr<const unit_frontier_t> u_frontier,
      std::shared_ptr<const b_frontier_t> b_frontier) const;
  // find the full command for a vertex
  Command command_from_vertex(
      const Vertex &vert, std::shared_ptr<const unit_frontier_t> u_frontier,
      std::shared_ptr<const b_frontier_t> prev_b_frontier) const;

  class TKET_EXPORT CommandIterator {
   private:
    Command current_command_;
    SliceIterator current_slice_iterator_;
    unsigned current_index_;
    Vertex current_vertex_;
    const Circuit *circ_;

    class Commandholder {
     private:
      Command current_command_;

     public:
      explicit Commandholder(Command command) : current_command_(command) {}
      Command operator*() const { return current_command_; }
    };

   public:
    explicit CommandIterator(const Circuit &circ);
    CommandIterator()
        : current_vertex_(boost::graph_traits<DAG>::null_vertex()),
          circ_(nullptr) {}

    Command operator*() const { return current_command_; }
    const Command *operator->() const { return &current_command_; }
    Vertex get_vertex() const { return current_vertex_; }
    bool operator==(const CommandIterator &other) const {
      return current_vertex_ == other.current_vertex_;
    }
    bool operator!=(const CommandIterator &other) const {
      return !(*this == other);
    }
    // A postfix increment operator overload
    Commandholder operator++(int);

    // A prefix increment operator overload
    CommandIterator &operator++();
  };

  CommandIterator begin() const;
  const CommandIterator end() const;
  static const CommandIterator nullcit;

  ///////////////////////
  // Setters and Getters//
  ///////////////////////

  /* circuit constructor methods */

  /**
   * Construct an empty circuit.
   */
  Circuit() : phase(0) {}

  /**
   * Construct an empty named circuit.
   *
   * @param name name of circuit
   */
  explicit Circuit(const std::string &name);

  // constructor for circuit with `n` qubits
  // O(n)
  explicit Circuit(
      unsigned n, const std::optional<std::string> _name = std::nullopt);

  // constructor for circuit with `n` qubits, plus initialise a classical
  // register of size `m`
  // ie `m` ClInput vertices connected to `m` ClOutputs
  explicit Circuit(
      unsigned n, unsigned m,
      const std::optional<std::string> _name = std::nullopt);

  // copy constructor
  // not including op_table merge: O(E+V+q), `E` edges, `V` vertices, `q` qubits
  Circuit(const Circuit &circ);

  /**
   * Constructor for an empty circuit with some given qubits/bits
   */
  Circuit(const qubit_vector_t &qubits, const bit_vector_t &bits) : Circuit() {
    for (const Qubit &q : qubits) {
      add_qubit(q);
    }
    for (const Bit &b : bits) {
      add_bit(b);
    }
  }

  // copy assignment. Moves boundary pointers.
  Circuit &operator=(const Circuit &other);

  /**
   * Run a suite of checks for internal circuit validity.
   *
   * Abort if any fail. All circuit methods may assume these conditions and
   * must ensure that they are preserved.
   */
  void assert_valid() const;

  /* getters */
  // returns the vector of input/output vertices to dag, ordered by register
  VertexVec all_inputs() const;
  VertexVec q_inputs() const;
  VertexVec c_inputs() const;
  VertexVec w_inputs() const;
  VertexVec r_inputs() const;
  VertexVec all_outputs() const;
  VertexVec q_outputs() const;
  VertexVec c_outputs() const;
  VertexVec w_outputs() const;
  VertexVec r_outputs() const;

  qubit_vector_t all_qubits() const;
  qubit_vector_t created_qubits() const;
  qubit_vector_t discarded_qubits() const;
  bit_vector_t all_bits() const;
  unit_vector_t all_units() const;

  /**
   * Returns a map from bits to their (left-to-right) column index in
   * ILO-BE readouts.
   */
  std::map<Bit, unsigned> bit_readout() const;

  /**
   * If a Measure op is the last operation (ignoring Barriers) on both its qubit
   * and bit, this will map that qubit id to the same readout index as the bit.
   * This is useful to extract from the circuit before compilation to correctly
   * interpret the readouts, and after compilation to identify how to apply
   * corrections for error mitigation.
   */
  std::map<Qubit, unsigned> qubit_readout() const;

  /**
   * If a Measure op is the last operation (ignoring Barriers) on both its qubit
   * and bit, this will map that qubit id to bit id. This is useful after
   * compilation to identify how to apply corrections for error mitigation.
   */
  std::map<Qubit, Bit> qubit_to_bit_map() const;

  /**
   * Returns whether or not a given qubit/bit exists in the circuit
   */
  bool contains_unit(const UnitID &id) const;

  Vertex get_in(const UnitID &id) const;
  Vertex get_out(const UnitID &id) const;

  /** Looks up an input/output vertex in boundary to find the associated unit */
  UnitID get_id_from_in(const Vertex &in) const;
  UnitID get_id_from_out(const Vertex &out) const;

  opt_reg_info_t get_reg_info(std::string reg_name) const;
  std::optional<std::string> get_wasm_file_uid() const;
  register_t get_reg(std::string reg_name) const;

  // returns the total number of vertices in dag
  // O(1)
  unsigned n_vertices() const;
  // returns the total number of inputs/outputs to dag (via iterating through
  // boundary map) O(1)
  unsigned n_qubits() const;
  unsigned n_bits() const;
  unsigned n_units() const;

  /**
   * Does the given qubit begin with a \ref OpType::Create in the DAG?
   */
  bool is_created(const Qubit &id) const;

  /**
   * Does the given qubit end with a \ref OpType::Discard in the DAG?
   */
  bool is_discarded(const Qubit &id) const;

  // returns the total number of non-boundary vertices in dag
  // O(1)
  unsigned n_gates() const;

  // given a vertex, returns a vector of all its successor vertices (no
  // duplicates) O(log(n!)), where `n` is number of outedges from `vert`
  // (ignoring hashtable collisions)
  VertexVec get_successors(const Vertex &vert) const;
  VertexVec get_successors_of_type(const Vertex &vert, EdgeType type) const;
  // O(log(n!)), where `n` is number of inedges of `vert` (ignoring hashtable
  // collisions) given a vertex, returns a vector of all its predecessor
  // vertices (no duplicates)
  VertexVec get_predecessors(const Vertex &vert) const;
  VertexVec get_predecessors_of_type(const Vertex &vert, EdgeType type) const;

  // O(1)
  unsigned n_edges() const;

  unsigned n_edges_of_type(const EdgeType &et) const;

  // return the ports corresponding to an edge
  // O(1)
  std::pair<port_t, port_t> get_ports(const Edge &edge) const;
  port_t get_source_port(const Edge &edge) const;
  port_t get_target_port(const Edge &edge) const;
  EdgeType get_edgetype(const Edge &edge) const;

  /**
   * All inward edges, ordered by port number
   * Every port has a single in-edge and they are numbered contiguously
   * Types of edges will be mixed according to vert's signature
   *
   * @param vert vertex
   */
  EdgeVec get_in_edges(const Vertex &vert) const;

  /**
   * All inward edges of given type, ordered by port number
   *
   * @param vert vertex
   * @param et edge type
   */
  EdgeVec get_in_edges_of_type(const Vertex &vert, EdgeType et) const;

  /**
   * Outward edges for linear (Quantum, Classical) types, ordered by port number
   * Vector has one entry per port in vert's signature; Boolean ports have
   * nullopt
   *
   * @param vert vertex
   */
  std::vector<std::optional<Edge>> get_linear_out_edges(
      const Vertex &vert) const;

  /**
   * @brief Get the linear edge corresponding to `e`
   *
   * If `e` is linear, return itself, otherwise (if `e` is Boolean), return
   * the corresponding Classical edge.
   *
   * EdgeType::Classical and EdgeType::Quantum are linear types (and thus left
   * unchanged), whereas EdgeType::Boolean is not.
   */
  Edge get_linear_edge(const Edge &e) const;

  /**
   * Outward edges for all types, ordered by port number
   * For classical ports, the Classical output is given, followed by any
   * Boolean outputs
   *
   * @param vert vertex
   */
  EdgeVec get_all_out_edges(const Vertex &vert) const;

  /**
   * All outward edges of given type, ordered by port number
   *
   * @param vert vertex
   * @param et edge type
   */
  EdgeVec get_out_edges_of_type(const Vertex &vert, EdgeType et) const;

  /**
   * All bundles of outward Boolean edges, ordered by port number
   *
   * Each member of the list is a list of edges sharing the same port
   *
   * @param vert vertex
   */
  std::vector<EdgeVec> get_b_out_bundles(
      const Vertex &vert) const;  // returned by port no.

  /**
   * All bundles of in Boolean edges, ordered by port number
   *
   * Each member of the list is a list of edges sharing the same port
   *
   * @param vert vertex
   */
  std::vector<EdgeVec> get_b_in_bundles(
      const Vertex &vert) const;  // returned by port no.

  /**
   * Total number of inward edges
   *
   * @param vert vertex
   */
  unsigned n_in_edges(const Vertex &vert) const;

  /**
   * Number of inward edges of a specific type
   */
  unsigned n_in_edges_of_type(const Vertex &vert, EdgeType et) const;

  /**
   * Total number of outward edges
   *
   * @param vert vertex
   */
  unsigned n_out_edges(const Vertex &vert) const;

  /**
   * Number of outward edges of a specific type
   */
  unsigned n_out_edges_of_type(const Vertex &vert, EdgeType et) const;

  /**
   * Number of ports expected on vertex based on Op signature
   */
  unsigned n_ports(const Vertex &vert) const;

  // O(n) n is the number of ports
  /**
   * @brief Get the edge targeting the nth input port at vert_to
   *
   * @param vert_to a vertex
   * @param n the input port number
   * @return the corresponding edge
   */
  Edge get_nth_in_edge(const Vertex &vert_to, const port_t &n) const;

  // O(n) n is the number of ports
  /**
   * @brief Get the edge originated from the nth output port at vert_from
   *
   * @param vert_from a vertex
   * @param n the output port number
   * @return the corresponding edge
   */
  Edge get_nth_out_edge(const Vertex &vert_from, const port_t &n) const;
  EdgeVec get_nth_b_out_bundle(const Vertex &vert_from, const port_t &n) const;

  /** True if no incident edges are @ref EdgeType::Classical */
  bool is_quantum_node(const Vertex &vert) const;

  /** True if no incident edges are @ref EdgeType::Quantum */
  bool is_classical_node(const Vertex &vert) const;

  // O(1)
  Vertex target(const Edge &e) const { return boost::target(e, dag); }
  // O(1)
  Vertex source(const Edge &e) const { return boost::source(e, dag); }

  // given a vertex, returns its associated op
  // O(1)
  const Op_ptr get_Op_ptr_from_Vertex(const Vertex &vert) const;
  void set_vertex_Op_ptr(const Vertex &vert, const Op_ptr &op);

  /**
   * Get the op group name (if any) associated with a vertex.
   */
  const std::optional<std::string> &get_opgroup_from_Vertex(
      const Vertex &vert) const;

  /**
   * Get the set of all opgroup names.
   */
  const std::unordered_set<std::string> get_opgroups() const;

  // O(1) (lookup in hashtable)
  OpDesc get_OpDesc_from_Vertex(const Vertex &vert) const;
  OpType get_OpType_from_Vertex(const Vertex &vert) const;
  op_signature_t get_Op_signature_from_Vertex(const Vertex &vert) const;

  /**
   * Given a Quantum or Classical in-edge to vert, returns the corresponding
   * out-edge, tracing the resource unit through the dag.
   * O(n), `n` is number of out edges of vert
   */
  Edge get_next_edge(const Vertex &vert, const Edge &in_edge)
      const;  // this doesnt need a vertex
  // O(n), `n` is number of in edges of vert
  Edge get_last_edge(const Vertex &vert, const Edge &out_edge) const;

  // given a vertex and corresponding in edge, returns next operation as its
  // vertex and edge O(n), `n` is number of out edges of vert
  std::pair<Vertex, Edge> get_next_pair(
      const Vertex &current_vertex, const Edge &inedge) const;
  // given a vertex and corresponding out edge, returns last op as its vertex
  // and edge
  // O(n), `n` is number of in edges of vert
  std::pair<Vertex, Edge> get_prev_pair(
      const Vertex &current_vertex, const Edge &outedge) const;

  /**
   * Detect whether a vertex corresponds to an initial node in the DAG.
   */
  bool detect_initial_Op(const Vertex &vertex) const;

  /**
   * Detect whether a vertex corresponds to a final node in the DAG.
   */
  bool detect_final_Op(const Vertex &vertex) const;

  /**
   * Detect whether a vertex corresponds to a boundary node in the DAG.
   */
  bool detect_boundary_Op(const Vertex &vertex) const;

  // returns true if given vertex is a single qubit, reversible gate
  // O(1)
  bool detect_singleq_unitary_op(const Vertex &vert) const;

  /**
   * Which Pauli, if any, commutes with the operation at a given vertex and port
   *
   * @param vert vertex
   * @param port port number at which Pauli should commute
   * @return a Pauli that commutes with the given operation
   * @retval std::nullopt no Pauli commutes (or operation is not a gate)
   * @retval Pauli::I every Pauli commutes
   */
  std::optional<Pauli> commuting_basis(const Vertex &vert, port_t port) const;

  /**
   * Whether the operation at a vertex commutes with a Pauli at the given port
   *
   * @param vert vertex
   * @param colour Pauli operation type
   * @param port port number at which Pauli may commute
   */
  bool commutes_with_basis(
      const Vertex &vert, const std::optional<Pauli> &colour,
      port_t port) const;

  /**
   * Convert all quantum and classical bits to use default registers.
   *
   * @return mapping from old to new unit IDs
   */
  unit_map_t flatten_registers();

  //_________________________________________________

  //////////////////////////////
  // Basic Circuit Manipulation//
  //////////////////////////////

  /**
   * O(V), `V` the number of vertices
   * removes all blank wires
   * @param keep_blank_classical_wires if true, empty classical wires are not
   * removed
   * @param remove_classical_only_at_end_of_register if true, empty classical
   * wires are not removed if they don't belong to a register or there are any
   * non-empty wires with a higher index in the same register
   */
  void remove_blank_wires(
      bool keep_blank_classical_wires = false,
      bool remove_classical_only_at_end_of_register = true);

  /**
   * Remove all gates in the circuits that are identities
   */
  void remove_noops();

  /**
   * Append an operation to the circuit.
   *
   * Appends vertex to the end of the given paths. Assumes the units already
   * exist in the circuit and boundary.
   *
   * @param op operation to append
   * @param args any Quantum, Boolean, or Classical inputs
   * @param opgroup name of associated operation group, if any
   *
   * @return the newly-added vertex in the circuit's DAG
   */
  // O(n alpha), where `n` size of qubits vector
  // the `alpha` comes from edge removal to rewire and is the number of edges in
  // the highest arity vertex
  template <class ID>
  Vertex add_op(
      const Op_ptr &op, const std::vector<ID> &args,
      std::optional<std::string> opgroup = std::nullopt);

  /**
   * Append an operation of a given type (with no parameters) to the circuit
   *
   * @param type type of operation to append
   * @param args any Quantum, Boolean, or Classical inputs
   * @param opgroup name of associated operation group, if any
   *
   * @return the newly-added vertex in the circuit's DAG
   */
  template <class ID>
  Vertex add_op(
      OpType type, const std::vector<ID> &args,
      std::optional<std::string> opgroup = std::nullopt) {
    return add_op(type, std::vector<Expr>{}, args, opgroup);
  }

  /**
   * Append an operation of a given type (with a parameter) to the circuit
   *
   * @param type type of operation to append
   * @param param operation parameter
   * @param args any Quantum, Boolean, or Classical inputs
   * @param opgroup name of associated operation group, if any
   *
   * @return the newly-added vertex in the circuit's DAG
   */
  template <class ID>
  Vertex add_op(
      OpType type, const Expr &param, const std::vector<ID> &args,
      std::optional<std::string> opgroup = std::nullopt) {
    return add_op(type, std::vector<Expr>{param}, args, opgroup);
  }

  /**
   * Append an operation of a given type (with parameters) to the circuit
   *
   * @param type type of operation to append
   * @param params operation parameters
   * @param args any Quantum, Boolean, or Classical inputs
   * @param opgroup name of associated operation group, if any
   *
   * @return the newly-added vertex in the circuit's DAG
   */
  template <class ID>
  Vertex add_op(
      OpType type, const std::vector<Expr> &params, const std::vector<ID> &args,
      std::optional<std::string> opgroup = std::nullopt) {
    if (is_metaop_type(type) || is_barrier_type(type)) {
      throw CircuitInvalidity(
          "Cannot add metaop or barrier. Please use `add_barrier` to add a "
          "barrier.");
    }
    return add_op(get_op_ptr(type, params, args.size()), args, opgroup);
  }

  /**
   * Add a measure operation from a qubit to a bit
   *
   * @param qubit qubit to measure
   * @param bit target of measurement
   *
   * @return vertex representing the measure operation
   */
  Vertex add_measure(const Qubit &qubit, const Bit &bit) {
    return add_op<UnitID>(OpType::Measure, {qubit, bit});
  }

  /**
   * Add a measure operation from a qubit to a bit on the default registers
   *
   * @param qubit qubit to measure
   * @param bit target of measurement
   *
   * @return vertex representing the measure operation
   */
  Vertex add_measure(unsigned qubit, unsigned bit) {
    return add_measure(Qubit(qubit), Bit(bit));
  }

  /**
   * Append a box to the circuit
   *
   * @param box box to append
   * @param args any Quantum, Boolean, or Classical inputs
   * @param opgroup name of associated operation group, if any
   *
   * @return the newly-added vertex in the circuit's DAG
   */
  template <class BoxT, class ID = unsigned>
  Vertex add_box(
      const BoxT &box, const std::vector<ID> &args,
      std::optional<std::string> opgroup = std::nullopt) {
    return add_op(std::make_shared<BoxT>(box), args, opgroup);
  }

  /**
   * Append a conditional operation to the circuit
   *
   * @param type type of operation to append
   * @param params parameters of operation
   * @param args any Quantum, Boolean, or Classical inputs
   * @param bits any Classical outputs
   * @param value value on which to condition operation (little-endian)
   * @param opgroup name of associated operation group, if any
   *
   * @return the newly-added vertex in the circuit's DAG
   */
  template <class ID>
  Vertex add_conditional_gate(
      OpType type, const std::vector<Expr> &params, const std::vector<ID> &args,
      const std::vector<ID> &bits, unsigned value,
      std::optional<std::string> opgroup = std::nullopt) {
    if (is_metaop_type(type)) {
      throw CircuitInvalidity("Cannot add a conditional metaop.");
    }
    if (is_barrier_type(type)) {
      throw CircuitInvalidity(
          "Please use 'add_conditional_barrier' to add a conditional barrier "
          "gate.");
    }
    Op_ptr cond = std::make_shared<Conditional>(
        get_op_ptr(type, params, (unsigned)args.size()), (unsigned)bits.size(),
        value);
    std::vector<ID> new_args = bits;
    new_args.insert(new_args.end(), args.begin(), args.end());
    return add_op(cond, new_args, opgroup);
  }

  Vertex add_barrier(
      const std::vector<unsigned> &qubits,
      const std::vector<unsigned> &bits = {}, const std::string &_data = "");

  Vertex add_barrier(const unit_vector_t &args, const std::string &_data = "");

  Vertex add_conditional_barrier(
      const std::vector<unsigned> &barrier_qubits,
      const std::vector<unsigned> &barrier_bits,
      const std::vector<unsigned> &condition_bits, unsigned value,
      const std::string &_data,
      std::optional<std::string> opgroup = std::nullopt);

  Vertex add_conditional_barrier(
      const unit_vector_t &barrier_args, const bit_vector_t &condition_bits,
      unsigned value, const std::string &_data,
      std::optional<std::string> opgroup = std::nullopt);

  /**
   * Add a postfix to a classical register name if the register exists
   * Example: tket_c results in tket_c_2 if tket_c and tket_c_1 both exist
   *
   * @param reg_name the base register name
   *
   * @return the incremented register name
   */
  std::string get_next_c_reg_name(const std::string &reg_name);

  Vertex add_assertion(
      const ProjectorAssertionBox &assertion_box,
      const std::vector<Qubit> &qubits,
      const std::optional<Qubit> &ancilla = std::nullopt,
      const std::optional<std::string> &name = std::nullopt);

  Vertex add_assertion(
      const StabiliserAssertionBox &assertion_box,
      const std::vector<Qubit> &qubits, const Qubit &ancilla,
      const std::optional<std::string> &name = std::nullopt);

  /**
   * Add a vertex to the DAG.
   *
   * O(1)
   *
   * @param op_ptr operation associated with vertex
   * @param opgroup name of associated operation group, if any
   *
   * @return the new vertex
   */
  Vertex add_vertex(
      const Op_ptr op_ptr, std::optional<std::string> opgroup = std::nullopt);

  /**
   * Add a vertex of given type (no parameters) to the DAG.
   *
   * O(1)
   *
   * @param type type of operation associated with vertex
   * @param opgroup name of associated operation group, if any
   *
   * @return the new vertex
   */
  Vertex add_vertex(
      const OpType &type, std::optional<std::string> opgroup = std::nullopt);

  // given vertices and desired in port for i2 and out port
  // for i1, adds edge between them
  // O(1)
  Edge add_edge(
      const VertPort &source, const VertPort &target, const EdgeType &type);

  // adds blank wire, originally intended to use all of architecture available
  // in routing a 'blank wire' is an input -> output path
  // O(n)
  void add_blank_wires(unsigned n);

  void add_qubit(const Qubit &id, bool reject_dups = true);
  void add_bit(const Bit &id, bool reject_dups = true);
  register_t add_q_register(std::string reg_name, unsigned size);
  register_t add_c_register(std::string reg_name, unsigned size);
  void add_wasm_register(std::size_t number_of_w = 1);
  void add_rng_register(std::size_t number_of_r = 1);

  /**
   * Create the given qubit in the zero state at the beginning of the circuit.
   *
   * The qubit must exist in the circuit already. This changes its initial
   * node in the DAG to @ref OpType::Create instead of @ref OpType::Input.
   * This is semantically equivalent to adding a @ref OpType::Reset operation
   * immediately after the input node.
   *
   * If the node is already @ref OpType::Create, the method does nothing.
   *
   * @param id qubit
   * @throws CircuitInvalidity if qubit not in circuit
   */
  void qubit_create(const Qubit &id);

  /** Call \ref qubit_create on all qubits. */
  void qubit_create_all();

  /**
   * Discard the given qubit at the end of the circuit.
   *
   * The qubit must exist in the circuit already. This changes its final node
   * in the DAG to @ref OpType::Discard instead of @ref OpType::Output. This
   * means that the qubit wire cannot be precomposed with a qubit wire in
   * another circuit that begins with an @ref OpType::Input.
   *
   * If the node is already @ref OpType::Discard, the method does nothing.
   *
   * @param id qubit
   * @throws CircuitInvalidity if qubit not in circuit
   */
  void qubit_discard(const Qubit &id);

  /** Call \ref qubit_discard on all qubits. */
  void qubit_discard_all();

  /**
   * Rename all the units according to the given mapping
   *
   * @return true iff circuit was modified
   */
  template <typename UnitA, typename UnitB>
  bool rename_units(const std::map<UnitA, UnitB> &qm);

  /** Automatically rewire holes when removing vertices from the circuit? */
  enum class GraphRewiring { Yes, No };

  /** Delete vertices from the DAG when removing them from the circuit? */
  enum class VertexDeletion { Yes, No };

  /** How to treat op groups when substituting in larger circuit */
  enum class OpGroupTransfer {
    Preserve, /**< keep them, error only for collisions */
    Remove,   /**< remove them */
    Disallow, /**< disallow them */
    Merge     /**< merge them, error for mismatched signature */
  };

  /** Merge boundaries when copying another circuit's DAG into a circuit? */
  enum class BoundaryMerge { Yes, No };

  // this could be templated or VertexSet version removed entirely?
  // if graph_rewiring is on, this is O(N n alpha), where N the size of surplus,
  // `n` the number of edges on the maximum arity vertex to be removed
  // Else, O(N)
  // if any of the vertices are boundaries, introduces additional factor of
  // times `q`, where `q` size of boundary/number of qubits
  void remove_vertices(
      const VertexSet &surplus, GraphRewiring graph_rewiring,
      VertexDeletion vertex_deletion);
  void remove_vertices(
      const VertexList &surplus, GraphRewiring graph_rewiring,
      VertexDeletion vertex_deletion);

  // given vertex, eradicates it from dag
  // if graph_rewiring is on, O(n alpha), where `n` is arity of vertex
  // else O(1)
  // if any of the vertices are boundaries, factor of `q` again
  void remove_vertex(
      const Vertex &deadvert, GraphRewiring graph_rewiring,
      VertexDeletion vertex_deletion);
  // sometimes want to remove a vertex from a circuit without destroying it
  // (eg to keep a container of vertices valid)

  /**
   * Removes a single edge from the dag
   */
  void remove_edge(const Edge &edge);

  /**
   * Rewires linear resources (Quantum or Classical) in place of pred
   * Adds new edges for Boolean
   * O(n alpha), where `n` is the number of edges in the cut
   */
  void rewire(
      const Vertex &new_vert, const EdgeVec &preds,
      const op_signature_t &types);

  //_________________________________________________

  ////////////////////
  // Macro Circuit Info//
  ////////////////////

  bool is_simple() const;
  bool default_regs_ok() const;

  /**
   * Count operations by type.
   *
   * Includes all types of operation, including initial and final nodes.
   *
   * @return map from op type to number of instances
   */
  std::map<OpType, unsigned> op_counts() const;

  unsigned count_gates(
      const OpType &op_type, const bool include_conditional = false) const;
  VertexSet get_gates_of_type(const OpType &op_type) const;

  /**
   * @brief Get all commands of a given type.
   *
   * @param op_type operation type
   *
   * @return list of commands of given type, in causal order
   */
  std::list<Command> get_commands_of_type(OpType op_type) const;

  /**
   * @brief Get the number of gates with the specified number of Qubits.
   *
   * Counts vertices, ignoring Input, Output, create, Discard, Reset,
   * Measure and Barrier.
   *
   * @param size Number of quantum edges a vertex has to be counted
   *
   * @return Number of vertices with given number of quantum edges in the
   * circuit
   */
  unsigned count_n_qubit_gates(unsigned size) const;

  // returns 'slices' of 'parallel' actions in dag as a vector encompassing
  // all vertices
  // O(D qlog^2(q!) alpha log(alpha!))
  // this is o(D q^3 log^2(q) alpha^2 log(alpha))
  SliceVec get_slices() const;

  // returns slices of parallel actions from the end to the front
  SliceVec get_reverse_slices() const;

  // starts at input edge and follows qubit path, returns first edge on path
  // with non single qubit target (or Output) O(D + alpha)
  Edge skip_irrelevant_edges(Edge current) const;

  // given current slice and a set of slices, returns the next slice
  // O(q log^2(q!) alpha log(alpha!))
  CutFrontier next_cut(
      std::shared_ptr<const unit_frontier_t> u_frontier,
      std::shared_ptr<const b_frontier_t> b_frontier,
      const std::function<bool(Op_ptr)> &skip_func = 0) const;

  // given current slice of quantum frontier, returns the next slice.
  // ignore classical and boolean edges
  CutFrontier next_q_cut(
      std::shared_ptr<const unit_frontier_t> u_frontier) const;

  /**
   * Depth of circuit.
   *
   * This is the number of vertices in the longest path through the DAG,
   * excluding boundary vertices and vertices representing barriers.
   *
   * O(D qlog^2(q!) alpha log(alpha!))
   *
   * @return depth
   */
  unsigned depth() const;

  /**
   * Depth of circuit restricting to one operation type.
   *
   * This is the number of vertices in the longest path through the sub-DAG
   * consisting of vertices representing operations of the given type.
   *
   * @return depth
   */
  unsigned depth_by_type(OpType _type) const;

  /**
   * Depth of circuit restricting to a set of operation types.
   *
   * This is the number of vertices in the longest path through the sub-DAG
   * consisting of vertices representing operations of the given types.
   *
   * @return depth
   */
  unsigned depth_by_types(const OpTypeSet &_types) const;

  /**
   * Depth of circuit restricting to 2-qubit operations, excluding barriers.
   *
   * This is the number of vertices in the longest path through the
   * sub-DAG consisting of vertices with 2 quantum wires,
   * excluding vertices representing barrier operations.
   *
   * @return depth
   */
  unsigned depth_2q() const;

  std::map<Vertex, unit_set_t> vertex_unit_map() const;
  std::map<Vertex, unsigned> vertex_depth_map() const;
  std::map<Vertex, unsigned> vertex_rev_depth_map() const;
  std::map<Edge, UnitID> edge_unit_map() const;

  /**
   * Construct a new circuit representing the given subcircuit. Each Boolean
   * edge entering the subcircuit is mapped to a unique classical Bit so we can
   * verify that any modifications made preserve the "read-only" nature.
   *
   * @param sc subcircuit specification
   *
   * @return new circuit representing a copy of the subcircuit
   */
  Circuit subcircuit(const Subcircuit &sc) const;

  /**
   * Construct a subcircuit consisting of a single vertex.
   *
   * @param v vertex
   *
   * @return subcircuit
   */
  Subcircuit singleton_subcircuit(const Vertex &v) const;

  // returns qubit path via vertices & inhabited port in vertices
  QPathDetailed unit_path(const UnitID &unit) const;  // vector<vertex,port>
  // returns a vector of each qubits path via qubit_path
  std::vector<QPathDetailed> all_qubit_paths() const;
  std::map<UnitID, QPathDetailed> all_unit_paths() const;
  // returns a basic qubit path consisting of just vertices
  VertexVec qubit_path_vertices(const Qubit &qubit) const;

  // returns a map from input qubit to output qubit on the same path
  qubit_map_t implicit_qubit_permutation() const;

  /**
   * Whether the circuit contains implicit wireswaps
   */
  bool has_implicit_wireswaps() const;

  /*
      Permute output boundary of circuit according to qubit map
      Assumes all circuit Qubits are mapped
  */
  void permute_boundary_output(const qubit_map_t &qm);

  /**
   * Equality operator
   *
   * Two circuits compare equal if they comprise the same sequence of
   * operators, with the ordering guaranteed by @ref get_commands, and if name,
   * phase, Qubits, Bits and implicit permutation also match.
   * By using the circuit_equality method, attributes other than commands
   * can be ignored in the equality check by being passed to the optional
   * except parameter.
   */
  enum class Check { Units, ImplicitPermutation, Phase, Name };
  // fine grained equality check, can set attributes to be ignored
  // optionally throws errors when mismatch found
  bool circuit_equality(
      const Circuit &other, const std::set<Check> &except = {},
      bool throw_error = true) const;
  bool operator==(const Circuit &other) const {
    return this->circuit_equality(other, {}, false);
  }

  /** @brief Checks causal ordering of vertices
   *
   * @param target the target vertex
   * @param from the source vertex
   * @param forward whether causality check is towards the future or reverse
   * @param v_to_depth depth map
   * @param v_to_units units map
   * @param strict whether causality check should be strict (i.e. from == target
   *               returns false) or not. Defaults to true
   */
  bool in_causal_order(
      const Vertex &target, const Vertex &from, bool forward,
      const std::map<Vertex, unsigned> &v_to_depth,
      const std::map<Vertex, unit_set_t> &v_to_units, bool strict = true) const;

  //___________________________________________________

  ////////////////////////////
  // Large Scale Manipulation//
  ////////////////////////////

  /**
   * Copy a circuit's DAG into the current circuit.
   *
   * Optionally updates the boundary of the composite circuit.
   *
   * Self-copy is not supported.
   *
   * @param c2 circuit to copy
   * @param boundary_merge whether to merge circuit boundaries
   * @param opgroup_transfer how to handle op group names
   *
   * @return map from vertices in inserted circuit to new inserted vertices
   */
  vertex_map_t copy_graph(
      const Circuit &c2, BoundaryMerge boundary_merge = BoundaryMerge::Yes,
      OpGroupTransfer opgroup_transfer = OpGroupTransfer::Merge);

  // O(E+V+q) -- E,V,q of c2
  void append(const Circuit &c2);
  // TODO:: Register-specific appending, probably be defining a register
  // renaming method
  void append_with_map(const Circuit &c2, const unit_map_t &qm);
  void append_qubits(
      const Circuit &c2, const std::vector<unsigned> &qubits,
      const std::vector<unsigned> &bits = {});

  // O(E+V+q) -- E,V,q of c2
  friend Circuit operator*(const Circuit &c1, const Circuit &c2);
  // given two circuits, adds second circuit to first sequentially by tying
  // qubits together
  // O(E1+V1+q1+E2+V2+q2) -- both circuits are copied here
  friend Circuit operator>>(const Circuit &c1, const Circuit &c2);

  // O(E+V+q) -- E,V,q of incirc
  void cut_insert(
      const Circuit &incirc, const EdgeVec &preds,
      const EdgeVec &b_future = {});  // naive insertion

  // Insert v2: Takes a subcircuit with valid boundary and replaces whatever
  // is in a hole
  // O(E+V+q+(n alpha)), where `n` the number of vertices to delete (regardless
  // of vertex_deletion) E,V,q are of `to_insert` Will fail if any classical
  // inputs/outputs of to_insert are discarded/trivial

  /**
   * Replace a subcircuit with a new circuit
   *
   * @param to_insert circuit to insert
   * @param to_replace subcircuit to replace
   * @param vertex_deletion whether to delete replaced vertices from the DAG
   * @param opgroup_transfer how to treat op groups in \p to_insert
   */
  void substitute(
      const Circuit &to_insert, const Subcircuit &to_replace,
      VertexDeletion vertex_deletion = VertexDeletion::Yes,
      OpGroupTransfer opgroup_transfer = OpGroupTransfer::Disallow);

  /**
   * Replace a vertex with a new circuit
   *
   * @param to_insert replacement circuit
   * @param to_replace vertex to replace
   * @param vertex_deletion whether to remove \p to_replace from the DAG
   * @param opgroup_transfer how to treat op groups in \p to_insert
   *
   * @pre \p to_replace has no Boolean inputs
   */
  void substitute(
      const Circuit &to_insert, const Vertex &to_replace,
      VertexDeletion vertex_deletion = VertexDeletion::Yes,
      OpGroupTransfer opgroup_transfer = OpGroupTransfer::Disallow);

  /**
   * Replace a conditional vertex with a new circuit.
   *
   * The new circuit replaces the op to be performed conditionally (i.e. the
   * conditions are not included in this circuit).
   *
   * @param to_insert replacement circuit
   * @param to_replace vertex with conditional op to replace
   * @param vertex_deletion whether to remove \p to_replace from the DAG
   * @param opgroup_transfer how to treat op groups in \p to_insert
   *
   * @pre \p to_insert should have no named op groups
   */
  void substitute_conditional(
      Circuit to_insert, const Vertex &to_replace,
      VertexDeletion vertex_deletion = VertexDeletion::Yes,
      OpGroupTransfer opgroup_transfer = OpGroupTransfer::Disallow);

  /**
   * Replace all explicit swaps (i.e. SWAP gates) with implicit swaps.
   *
   * The boundaries are not updated. Implicit permutations introduced in this
   * way can be obtained using the \ref implicit_qubit_permutation method.
   *
   * @param replace_tk2_equivalents If true, also replace TK2 gates equivalent
   * to SWAPs.
   *
   * O(V)
   *
   * @return whether any swaps were replaced
   */
  bool replace_SWAPs(bool replace_tk2_equivalents = false);

  /**
   * this function replaces an implicit wire swap between the two given qubits
   * with a SWAP gate or three CX operations
   *
   * @param first qubits to add the wireswap on
   * @param second qubits to add the wireswap on
   * @param using_cx use three CX gates instead of a SWAP gate
   *
   * O(c)
   */
  void replace_implicit_wire_swap(
      const Qubit first, const Qubit second, bool using_cx = true);

  /**
   * replaces all implicit wire swaps with SWAP gates
   *
   * O(n_qubits)
   */
  void replace_all_implicit_wire_swaps();

  // O(E+V+q)
  Circuit dagger() const;

  Circuit transpose() const;

  /**
   * Substitute all vertices matching the given op with the given circuit
   *
   * @param to_insert circuit to insert
   * @param op operation to match
   *
   * @return whether any vertices were replaced
   *
   * @pre \p to_insert should have no named op groups
   */
  bool substitute_all(const Circuit &to_insert, const Op_ptr op);

  /**
   * Substitute all operations matching the given name with the given circuit.
   *
   * Any named operations in the inserted circuit retain their names in the
   * new circuit.
   *
   * @param to_insert circuit to insert
   * @param opname name of operations to replace
   *
   * @return whether any replacements were made
   *
   * @pre Named operations in the inserted circuit must have the same
   * signature as any named operations in the main circuit with the same name.
   */
  bool substitute_named(const Circuit &to_insert, const std::string opname);

  /**
   * Substitute all operations matching the given name with the given op.
   *
   * The inserted operations retain the name of the substituted ones.
   *
   * @param to_insert operation to insert
   * @param opname name of operations to replace
   *
   * @return whether any replacements were made
   */
  bool substitute_named(Op_ptr to_insert, const std::string opname);

  /**
   * Substitute all operations matching the given name with the given box.
   *
   * The inserted boxes retain the name of the substituted ones.
   *
   * @param to_insert box to insert
   * @param opname name of operations to replace
   *
   * @return whether any replacements were made
   */
  template <class BoxT>
  bool substitute_named(const BoxT &to_insert, const std::string opname) {
    // Do nothing if opname not present
    if (opgroupsigs.find(opname) == opgroupsigs.end()) {
      return false;
    }

    // Check signatures match
    op_signature_t sig = opgroupsigs[opname];
    if (to_insert.get_signature() != sig) {
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
    c.add_box(to_insert, args, opname);
    for (const Vertex &v : to_replace) {
      substitute(c, v, VertexDeletion::Yes, OpGroupTransfer::Merge);
    }

    return !to_replace.empty();
  }

  /**
   * Adds a condition to every op in the circuit.
   *
   * If the circuit has a global phase, this is expressed by appending a
   * conditional Phase operation.
   *
   * Will throw a CircuitInvalidity error if the circuit contains implicit
   * wireswaps (as these cannot be applied conditionally) or writes to the
   * condition bits at any point.
   *
   * @param bits Set of bits to condition the execution
   * @param value Little-endian target value for the condition bits (e.g. value
   * 2 (10b) means bits[0] must be 0 and bits[1] must be 1)
   */
  Circuit conditional_circuit(const bit_vector_t &bits, unsigned value) const;

  /**
   * Replaces one vertex by applying \ref Box::to_circuit
   *
   * @return whether the vertex holds a box or a conditional box
   */
  bool substitute_box_vertex(
      Vertex &vert, VertexDeletion vertex_deletion,
      const std::unordered_set<OpType> &excluded_types = {},
      const std::unordered_set<std::string> &excluded_opgroups = {});

  /**
   * Recursively replace each \ref Box operation by applying \ref
   * Box::to_circuit
   *
   * Arguments specify ways to filter which boxes are decomposed. A box must
   * satisfy ALL filters in order to be decomposed, e.g. if all are set,
   * decompose those boxes whose type is in included_types but not in
   * excluded_types, and also its opgroup is in included_opgroups but not in
   * excluded_opgroups
   *
   * @param excluded_types box types excluded from decomposition
   * @param excluded_opgroups opgroups excluded from decomposition
   * @param included_types optional, only decompose these box types
   * @param included_opgroups optional, only decompose these opgroups
   *
   * @return whether any replacements were made
   */
  bool decompose_boxes_recursively(
      const std::unordered_set<OpType> &excluded_types = {},
      const std::unordered_set<std::string> &excluded_opgroups = {},
      const std::optional<std::unordered_set<OpType>> &included_types =
          std::nullopt,
      const std::optional<std::unordered_set<std::string>> &included_opgroups =
          std::nullopt);

  /////////////////
  // Other Methods//
  /////////////////

  void symbol_substitution(const symbol_map_t &symbol_map);
  void symbol_substitution(
      const std::map<Sym, double, SymEngine::RCPBasicKeyLess> &symbol_map);
  void symbol_substitution(const SymEngine::map_basic_basic sub_map);

  /** Set of all free symbols occurring in operation parameters. */
  const SymSet free_symbols() const;

  /** Whether the circuit contains any symbolic parameters */
  bool is_symbolic() const;

  // from Circuit to various output formats
  void to_graphviz_file(const std::string &filename) const;
  void to_graphviz(std::ostream &out) const;
  std::string to_graphviz_str() const;
  // output stream overload
  friend std::ostream &operator<<(std::ostream &out, const Circuit &circ);

  std::string to_latex_str() const;
  void to_latex_file(const std::string &filename) const;

  void extract_slice_segment(unsigned slice_one, unsigned slice_two);

  /* 'backends' for tket */

  /**
   * Get the sequence of commands comprising the circuit
   *
   * The order is determined first by the temporal order in the circuit and
   * secondly by the register names and indices.
   *
   * Running time is \f$ O(D q^2 \log^2(q!)) \f$ where \f$ q \f$ is the number
   * of qubits and \f$ D \f$ is the circuit depth.
   */
  std::vector<Command> get_commands() const;

  /**
   * All vertices of the DAG.
   *
   * @return vector of vertices
   */
  std::vector<Vertex> all_vertices() const;

  /**
   * Set the vertex indices in the DAG.
   *
   * Has no effect on circuit semantics, so "morally" const.
   */
  void index_vertices() /*const*/;

  /**
   * All vertices of the DAG, topologically sorted.
   *
   * This method is "morally" const, but it sets the vertex indices in the DAG.
   *
   * @return vector of vertices in a topological (causal) order
   */
  std::vector<Vertex> vertices_in_order() /*const*/;

  /**
   * Index the vertices according to their ordering in the DAG.
   *
   * Does not set the indices in the DAG: use @ref index_vertices for that.
   *
   * @return index map from Vertex to int
   */
  IndexMap index_map() const;

  /**
   * Return a partition into connected convex subcircuits of the set of ops
   * satisfying the given criterion.
   *
   * @param criterion criterion satisfied by all ops in all subcircuits
   *
   * @return vector of disjoint vertex sets whose union includes all ops
   *   satisfying the criterion.
   */
  std::vector<VertexSet> get_subcircuits(
      std::function<bool(Op_ptr)> criterion) /*const*/;

  /**
   * Constructs a @ref Subcircuit with a given vertex set.
   *
   * The vertex set is assumed to be connected and convex. (This is not
   * checked.)
   *
   * @param verts set of vertices
   *
   * @return corresponding @ref Subcircuit
   */
  Subcircuit make_subcircuit(const VertexSet &verts) const;

  /**
   * Get the global phase offset as a multiple of pi (in the range [0,2)).
   *
   * This is not meaningful for circuits with classical interaction.
   *
   * @return global phase
   */
  Expr get_phase() const;

  /**
   * Adds a global phase to the circuit
   *
   * @param a phase to add, as a multiple of pi
   */
  void add_phase(Expr a);

  /**
   * Get the name of the circuit.
   *
   * @return name
   */
  std::optional<std::string> get_name() const { return name; };

  /**
   * Set the name of the circuit
   *
   * @param _name name string
   */
  void set_name(const std::string _name) { name = _name; };

  const Op_ptr command2op(Command &com);

  /**
   * Evaluate a classical circuit on given inputs.
   *
   * The circuit must have only ClassicalTransform and SetBits operations. The
   * keys of the input map must correspond to the bits of the circuit.
   *
   * @param values input values
   * @return output values
   */
  std::map<Bit, bool> classical_eval(const std::map<Bit, bool> &values) const;

  /* class members */
  // currently public (no bueno)
  DAG dag; /** Representation as directed graph */
  boundary_t boundary;
  std::vector<WasmState> wasmwire;
  std::vector<RngState> rngwire;
  std::size_t _number_of_wasm_wires = 0;
  std::size_t _number_of_rng_wires = 0;

  /**
   * Calculate the overall resources of the circuit.
   *
   * This takes account of the data stored in each \ref DummyBox within the
   * circuit, as well as other gates, to compute upper and lower bounds.
   *
   * @return bounds on resources of the circuit
   */
  ResourceData get_resources() /*const*/;

 private:
  std::optional<std::string>
      name;   /** optional string name descriptor for human identification*/
  Expr phase; /**< Global phase applied to circuit */

  /** Signature associated with each named operation group */
  std::map<std::string, op_signature_t> opgroupsigs;
};

TKET_EXPORT Circuit operator>>(const Circuit &c1, const Circuit &c2);

JSON_DECL(Circuit)

/** Templated method definitions */

template <typename UnitA, typename UnitB>
bool Circuit::rename_units(const std::map<UnitA, UnitB> &qm) {
  // Can only work for Unit classes
  static_assert(std::is_base_of<UnitID, UnitA>::value);
  static_assert(std::is_base_of<UnitID, UnitB>::value);
  // Unit types must be related, so cannot rename e.g. Bits to Qubits
  static_assert(
      std::is_base_of<UnitA, UnitB>::value ||
      std::is_base_of<UnitB, UnitA>::value);
  std::map<UnitID, BoundaryElement> new_elems;
  bool modified = false;
  std::map<Bit, Bit> bm;
  for (const std::pair<const UnitA, UnitB> &pair : qm) {
    boundary_t::iterator found = boundary.get<TagID>().find(pair.first);
    if (found == boundary.get<TagID>().end()) {
      std::stringstream ss;
      ss << "unit " << pair.first.repr() << " not found in circuit";
      tket_log()->warn(ss.str());
      continue;
    }

    opt_reg_info_t target_reg_info = get_reg_info(pair.second.reg_name());
    if (target_reg_info) {
      if (target_reg_info.value().first != found->type())
        throw CircuitInvalidity(
            "Incompatible registers: " + pair.first.reg_name() + " and " +
            pair.second.reg_name());
      if (target_reg_info.value().second != pair.second.reg_dim())
        throw CircuitInvalidity(
            "Existing register " + pair.second.reg_name() +
            " cannot support id: " + pair.second.repr());
    }
    std::pair<std::map<UnitID, BoundaryElement>::iterator, bool> inserted =
        new_elems.insert({pair.second, {pair.second, found->in_, found->out_}});
    if (!inserted.second)
      throw CircuitInvalidity(
          "Mapping two units to the same id: " + pair.second.repr());
    modified = true;
    boundary.erase(found);
    if (pair.first.type() == UnitType::Bit) {
      bm.insert({Bit(pair.first), Bit(pair.second)});
    }
  }
  for (const std::pair<const UnitID, BoundaryElement> &pair : new_elems) {
    std::pair<boundary_t::iterator, bool> added = boundary.insert(pair.second);
    if (!added.second)
      throw CircuitInvalidity(
          "Unit already exists in circuit: " + pair.first.repr());
    TKET_ASSERT(modified);
  }
  return modified;
}

template <>
Vertex Circuit::add_op<unsigned>(
    const Op_ptr &op, const std::vector<unsigned> &args,
    std::optional<std::string> opgroup);

template <class ID>
Vertex Circuit::add_op(
    const Op_ptr &op, const std::vector<ID> &args,
    std::optional<std::string> opgroup) {
  static_assert(std::is_base_of<UnitID, ID>::value);
  op_signature_t sig = op->get_signature();

  // check if there are wasm or rng wires in the signature
  unsigned count_wasm_sig = 0;
  unsigned count_rng_sig = 0;
  for (EdgeType e : sig) {
    if (e == EdgeType::WASM) {
      ++count_wasm_sig;
    } else if (e == EdgeType::RNG) {
      ++count_rng_sig;
    }
  }

  if (sig.size() != args.size()) {
    throw CircuitInvalidity(
        std::to_string(args.size()) + " args provided, but " + op->get_name() +
        " requires " + std::to_string(sig.size()));
  }

  if (opgroup) {
    auto opgroupsig = opgroupsigs.find(opgroup.value());
    if (opgroupsig != opgroupsigs.end()) {
      if (opgroupsig->second != sig) {
        throw CircuitInvalidity("Mismatched signature for operation group");
      }
    } else {
      opgroupsigs[opgroup.value()] = sig;
    }
  }

  add_wasm_register(count_wasm_sig);
  add_rng_register(count_rng_sig);

  unit_set_t write_arg_set;
  EdgeVec preds;
  for (unsigned i = 0; i < args.size(); ++i) {
    const UnitID &arg = args[i];
    if (sig[i] != EdgeType::Boolean) {
      if (write_arg_set.find(arg) != write_arg_set.end())
        throw CircuitInvalidity(
            "Multiple operation arguments reference " + arg.repr());
      write_arg_set.insert(arg);
    }
    Vertex out_vert = get_out(arg);
    Edge pred_out_e = get_nth_in_edge(out_vert, 0);
    preds.push_back(pred_out_e);
  }

  Vertex new_v;
  OpType optype = op->get_type();
  if (optype == OpType::CnRy && args.size() == 1) {
    new_v = add_vertex(
        get_op_ptr(OpType::Ry, as_gate_ptr(op)->get_params()), opgroup);
  } else if (optype == OpType::CnRx && args.size() == 1) {
    new_v = add_vertex(
        get_op_ptr(OpType::Rx, as_gate_ptr(op)->get_params()), opgroup);
  } else if (optype == OpType::CnRz && args.size() == 1) {
    new_v = add_vertex(
        get_op_ptr(OpType::Rz, as_gate_ptr(op)->get_params()), opgroup);
  } else if (optype == OpType::CnX && args.size() == 1) {
    new_v = add_vertex(get_op_ptr(OpType::X), opgroup);
  } else if (optype == OpType::CnZ && args.size() == 1) {
    new_v = add_vertex(get_op_ptr(OpType::Z), opgroup);
  } else if (optype == OpType::CnY && args.size() == 1) {
    new_v = add_vertex(get_op_ptr(OpType::Y), opgroup);
  } else {
    new_v = add_vertex(op, opgroup);
  }
  rewire(new_v, preds, sig);
  return new_v;
}

}  // namespace tket
