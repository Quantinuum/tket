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

#include <atomic>

#include "Transform.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Clifford/UnitaryTableau.hpp"
#include "tket_export.h"

namespace tket {

namespace Transforms {

namespace GreedyPauliSimp {

class GreedyPauliSimpError : public std::logic_error {
 public:
  explicit GreedyPauliSimpError(const std::string& message)
      : std::logic_error(message) {}
};

/**
 * @brief Types of 2-qubit entangled Clifford gates
 *
 */
enum class TQEType : unsigned {
  XX = 0,
  XY,
  XZ,
  YX,
  YY,
  YZ,
  ZX,
  ZY,
  ZZ,
};

enum class PauliNodeType {
  // Pauli rotation
  PauliRotation,
  // Defines how a Pauli X and a Pauli Z on the same qubit
  // get propagated from right to left through a Clifford operator.
  PauliPropagation,
  // Conditional Pauli rotations
  ConditionalBlock,
  // Classical operation
  ClassicalNode,
  // Mid-circuit measurement
  MidMeasure,
  // Reset
  Reset,
};

/**
 * @brief The type of a pair of Pauli letters defined by
    their commutation relation
 *
 */
enum class CommuteType : unsigned {
  // Both are (I)dentity
  I,
  // (A)nti-commute
  A,
  // (C)ommute and not both identity
  C,
};

enum class BitType : unsigned {
  READ,
  WRITE,
};

/**
 * @brief Struct for 2-qubit entangled Clifford gates
 *
 */
struct TQE {
  TQEType type;
  unsigned a;
  unsigned b;
  bool operator<(const TQE& other) const {
    return std::tie(type, a, b) < std::tie(other.type, other.a, other.b);
  }
};

/**
 * @brief Struct for 2-qubit rotation gates
 *
 */
struct Rotation2Q {
  Pauli p_a;
  Pauli p_b;
  unsigned a;
  unsigned b;
  Expr angle;
  unsigned index;
  bool operator<(const Rotation2Q& other) const { return index < other.index; }
};

/**
 * @brief Commutation information of a node specified by a list of
 * Pauli strings along with classical READs and WRITEs.
 */
struct CommuteInfo {
  std::vector<std::vector<Pauli>> paulis;
  // We use UnitID to differentiate between Bit, WasmState and RngState
  std::vector<std::pair<UnitID, BitType>> bits_info;
};

/**
 * @brief Base class for nodes in the Greedy Pauli graph
 *
 */
class PauliNode {
 public:
  virtual PauliNodeType get_type() const = 0;
  virtual unsigned tqe_cost() const = 0;
  virtual int tqe_cost_increase(const TQE& tqe) const = 0;
  virtual void update(const TQE& tqe) = 0;
  virtual void update(const OpType& sq_cliff, const unsigned& a);
  virtual void swap(const unsigned& a, const unsigned& b);
  virtual CommuteInfo get_commute_info() const = 0;
  virtual std::vector<TQE> reduction_tqes() const = 0;
  virtual ~PauliNode();
};

typedef std::shared_ptr<PauliNode> PauliNode_ptr;

/**
 * @brief Base class for nodes defined by a single Pauli string
 */
class SingleNode : public PauliNode {
 public:
  /**
   * @brief Construct a new SinglePauliNode object.
   *
   * @param string the Pauli string
   * @param sign sign of the Pauli string
   */
  SingleNode(std::vector<Pauli> string, bool sign);

  /**
   * @brief Number of TQEs required to reduce the weight to 1
   *
   * @return unsigned
   */
  unsigned tqe_cost() const override;

  /**
   * @brief Number of TQEs would required to reduce the weight to 1
   * after the given TQE is applied
   *
   * @return unsigned
   */
  int tqe_cost_increase(const TQE& tqe) const override;

  /**
   * @brief Update the Pauli string with a TQE gate
   *
   * @param tqe
   */
  void update(const TQE& tqe) override;

  /**
   * @brief Return all possible TQE gates that will reduce the tqe cost by 1
   *
   * @return std::vector<std::tuple<TQEType, unsigned, unsigned>>
   */
  std::vector<TQE> reduction_tqes() const override;

  /**
   * @brief Return the index and value of the first non-identity
   *
   * @return std::pair<unsigned, Pauli>
   */
  std::pair<unsigned, Pauli> first_support() const;

  bool sign() const { return sign_; };

  const std::vector<Pauli>& string() const { return string_; };

 protected:
  std::vector<Pauli> string_;
  bool sign_;
  // extra cached data used by greedy synthesis
  unsigned weight_;
};

/**
 * @brief Base class for nodes defined by a pair of anti-commuting Pauli strings
 */
class ACPairNode : public PauliNode {
 public:
  /**
   * @brief Construct a new ACPairNode object
   *
   * @param z_string
   * @param x_string
   * @param z_sign
   * @param x_sign
   */
  ACPairNode(
      std::vector<Pauli> z_string, std::vector<Pauli> x_string, bool z_sign,
      bool x_sign);

  /**
   * @brief Number of TQEs required to reduce the weight to 1
   *
   * @return unsigned
   */
  unsigned tqe_cost() const override;

  /**
   * @brief Number of additional TQEs would required to reduce the weight to 1
   * after the given TQE is applied
   *
   * @param tqe
   * @return int
   */
  int tqe_cost_increase(const TQE& tqe) const override;

  /**
   * @brief Update the support vector with a TQE gate
   *
   * @param tqe
   */
  void update(const TQE& tqe) override;

  /**
   * @brief Update the support vector with a single-qubit Clifford gate
   *
   * @param sq_cliff
   * @param a
   */
  void update(const OpType& sq_cliff, const unsigned& a) override;

  /**
   * @brief Update the support vector with a SWAP gate
   *
   * @param a
   * @param b
   */
  void swap(const unsigned& a, const unsigned& b) override;

  /**
   * @brief Return all possible TQE gates that will reduce the tqe cost
   *
   * @return std::vector<TQE>
   */
  std::vector<TQE> reduction_tqes() const override;

  /**
   * @brief Return the index and value of the first anti-commute entry
   */
  std::tuple<unsigned, Pauli, Pauli> first_support() const;

  bool z_sign() const { return z_sign_; };

  bool x_sign() const { return x_sign_; };

  const std::vector<Pauli>& z_string() const { return z_string_; };

  const std::vector<Pauli>& x_string() const { return x_string_; };

 protected:
  std::vector<Pauli> z_string_;
  std::vector<Pauli> x_string_;
  bool z_sign_;
  bool x_sign_;
  // extra cached data used by greedy synthesis
  std::vector<CommuteType> commute_type_vec_;
  unsigned n_commute_entries_;
  unsigned n_anti_commute_entries_;
};

/**
 * @brief Black box node for classical Ops
 */
class ClassicalNode : public PauliNode {
 public:
  ClassicalNode(std::vector<UnitID> args, Op_ptr op);

  PauliNodeType get_type() const override {
    return PauliNodeType::ClassicalNode;
  };

  unsigned tqe_cost() const override { return 0; };
  int tqe_cost_increase(const TQE& /*tqe*/) const override { return 0; };
  void update(const TQE& /*tqe*/) override { return; };
  std::vector<TQE> reduction_tqes() const override { return {}; };
  std::vector<UnitID> args() const { return args_; };
  Op_ptr op() const { return op_; };

  CommuteInfo get_commute_info() const override;

 protected:
  const std::vector<UnitID> args_;
  const Op_ptr op_;
};

/**
 * @brief A Pauli exponential defined by a dense Pauli string
 * and a rotation angle
 */
class PauliRotation : public SingleNode {
 public:
  /**
   * @brief Construct a new PauliRotation object.
   *
   * @param string the Pauli string
   * @param sign the sign of the Pauli string
   * @param theta the rotation angle in half-turns
   */
  PauliRotation(std::vector<Pauli> string, bool sign, Expr theta);

  PauliNodeType get_type() const override {
    return PauliNodeType::PauliRotation;
  };

  Expr angle() const { return sign_ ? theta_ : -theta_; };

  CommuteInfo get_commute_info() const override;

 protected:
  const Expr theta_;
};

/**
 * @brief Measurement that has quantum or classical successors
 */
class MidMeasure : public SingleNode {
 public:
  /**
   * @brief Construct a new Mid Measure object
   *
   * @param string dense Pauli string
   * @param sign the sign of the Pauli string
   * @param bit readout bit
   */
  MidMeasure(std::vector<Pauli> string, bool sign, unsigned bit);

  PauliNodeType get_type() const override { return PauliNodeType::MidMeasure; };
  CommuteInfo get_commute_info() const override;
  unsigned bit() const { return bit_; };

 protected:
  const unsigned bit_;
};

/**
 * @brief Conditional block for rotations
 */
class ConditionalBlock : public PauliNode {
 public:
  /**
   * @brief Construct a new Conditional Block object
   *
   * @param rotations Pauli rotations
   * @param cond_bits conditional bits
   * @param cond_value conditional value
   */
  ConditionalBlock(
      std::vector<std::tuple<std::vector<Pauli>, bool, Expr>> rotations,
      std::vector<unsigned> cond_bits, unsigned cond_value);

  /**
   * @brief Sum of tqe_cost for each Pauli rotation
   *
   * @return unsigned
   */
  unsigned tqe_cost() const override;

  /**
   * @brief Sum of tqe_cost for each Pauli rotation after the given TQE is
   * applied
   *
   * @param tqe
   * @return unsigned
   */
  int tqe_cost_increase(const TQE& tqe) const override;

  /**
   * @brief Update the all Pauli rotations with the given TQE
   *
   * @param tqe
   */
  void update(const TQE& tqe) override;

  std::vector<TQE> reduction_tqes() const override { return {}; };

  std::vector<unsigned> cond_bits() const { return cond_bits_; };
  unsigned cond_value() const { return cond_value_; };

  PauliNodeType get_type() const override {
    return PauliNodeType::ConditionalBlock;
  };

  CommuteInfo get_commute_info() const override;

  void append(const ConditionalBlock& other);

  const std::vector<std::tuple<std::vector<Pauli>, bool, Expr>>& rotations()
      const {
    return rotations_;
  };

 protected:
  std::vector<std::tuple<std::vector<Pauli>, bool, Expr>> rotations_;
  const std::vector<unsigned> cond_bits_;
  const unsigned cond_value_;
  // extra cached data used by greedy synthesis
  unsigned total_weight_;
};

/**
 * @brief Defines how a Pauli X and a Pauli Z on the same qubit
 * get propagated from right to left through a Clifford operator.
 * A n-qubit Clifford operator is completely defined by n such propagations
 * with one on each qubit. A PauliPropagation also corresponds to a row in
 * a Clifford tableau
 */
class PauliPropagation : public ACPairNode {
 public:
  /**
   * @brief Construct a new PauliPropagation object
   *
   * @param z_string propagated Pauli Z
   * @param x_string propagated Pauli X
   * @param z_sign the sign of z_string
   * @param x_sign the sign of x_string
   * @param qubit_index i.e. row index
   */
  PauliPropagation(
      std::vector<Pauli> z_string, std::vector<Pauli> x_string, bool z_sign,
      bool x_sign, unsigned qubit_index);

  PauliNodeType get_type() const override {
    return PauliNodeType::PauliPropagation;
  };

  CommuteInfo get_commute_info() const override;

  unsigned qubit_index() const { return qubit_index_; };

 private:
  const unsigned qubit_index_;
};

/**
 * @brief Reset operation defined by a pair of anti-commuting strings
 * For example, a tket Reset OpType can be defined as a Z/X pair. The Pauli Z
 * can be seen as a Z-basis measurement, and the Pauli X can be seen as the post
 * measurement conditional X gate.
 *
 */
class Reset : public ACPairNode {
 public:
  /**
   * @brief Construct a new Reset object
   *
   * @param z_string
   * @param x_string
   * @param z_sign
   * @param x_sign
   */
  Reset(
      std::vector<Pauli> z_string, std::vector<Pauli> x_string, bool z_sign,
      bool x_sign);

  PauliNodeType get_type() const override { return PauliNodeType::Reset; };
  CommuteInfo get_commute_info() const override;
};

typedef boost::adjacency_list<
    boost::listS, boost::listS, boost::bidirectionalS,
    // indexing needed for algorithms such as topological sort
    boost::property<boost::vertex_index_t, int, PauliNode_ptr>>
    GPDAG;

typedef boost::graph_traits<GPDAG>::vertex_descriptor GPVert;
typedef boost::graph_traits<GPDAG>::edge_descriptor GPEdge;

typedef sequence_set_t<GPVert> GPVertSet;
typedef sequence_set_t<GPEdge> GPEdgeSet;

typedef boost::adj_list_vertex_property_map<
    GPDAG, int, int&, boost::vertex_index_t>
    GPVIndex;

/**
 * @brief Pauli graph structure for GreedyPauliSimp.
 *
 * A DAG is used to store all operations except for the end-of-circuit Clifford
 * and end-of-circuit measurements. The vertices consist of Pauli rotations,
 * mid-circuit measurements, resets, conditional Pauli rotations, and classical
 * operations. Edges represent gate dependencies, where two nodes commute if
 * they commute on both quantum and classical wires.
 *
 * - Quantum commutation: Nodes commute if all Pauli strings in one node
 *   commute with all strings in the other.
 * - Classical commutation: Nodes commute if they do not share classical
 *   bits, or if they only read from shared bits.
 *
 * End-of-circuit measurements are stored as a map from integers to integers.
 * These measurements are kept separate (i.e., after the final Clifford) so
 * optimisation around them can later be handled by
 * `CliffordPushThroughMeasures`.
 *
 * The final Clifford operator is stored using a `UnitaryRevTableau`. Note that
 * `UnitaryRevTableau` is chosen over `PauliPropagations` due to the
 * availability of existing update methods.
 */
class GPGraph {
 public:
  /** Construct an GPGraph from the qubits and bits */
  GPGraph(qubit_vector_t qubits, bit_vector_t bits);

  GPVertSet get_successors(const GPVert& vert) const;

  GPVertSet get_predecessors(const GPVert& vert) const;

  /**
   * All vertices of the DAG, topologically sorted.
   *
   * This method is "morally" const, but it sets the vertex indices in the DAG.
   *
   * @return vector of vertices in a topological (causal) order
   */
  std::vector<GPVert> vertices_in_order() const;

  std::tuple<
      std::vector<std::vector<PauliNode_ptr>>, std::vector<PauliNode_ptr>,
      boost::bimap<unsigned, unsigned>>
  get_sequence(std::shared_ptr<std::atomic<bool>> stop_flag);

  /**
   * Applies the given gate to the end of the graph.
   * Clifford gates transform the tableau.
   * Non-Clifford gates and conditional Clifford gates are transformed
   * into PauliNodes by the tableau and added
   * to the graph.
   */
  void apply_gate_at_end(
      const Command& cmd, bool conditional = false,
      std::vector<unsigned> cond_bits = {}, unsigned cond_value = 0);

 private:
  /**
   * Add a Pauli rotation to the graph
   * If the angle is non-Clifford or if conditional is true then add to the DAG
   * as a PauliRotation node, otherwise update the tableau.
   */
  void apply_paulis_at_end(
      const std::vector<std::pair<std::vector<Pauli>, Expr>>& rotations,
      const qubit_vector_t& qbs, bool conditional = false,
      std::vector<unsigned> cond_bits = {}, unsigned cond_value = 0);

  /**
   * Add a node to the DAG and check if it can be merged with another node.
   */
  void apply_node_at_end(PauliNode_ptr& node);

  /**
   * The dependency graph of Pauli nodes
   *
   * This is mutated by \ref vertices_in_order which indexes the vertices
   * without changing the structure.
   */
  mutable GPDAG graph_;
  const unsigned n_qubits_;
  const unsigned n_bits_;

  /** The tableau of the Clifford effect of the circuit */
  UnitaryRevTableau cliff_;
  /** The record of measurements at the very end of the circuit */
  boost::bimap<unsigned, unsigned> end_measures_;

  GPVertSet start_line_;
  GPVertSet end_line_;
};

/**
 * @brief Convert a unordered set of SymPauliTensor into a set of PauliRotations
 * followed by a set of PauliPropagations
 *
 * @param unordered_set
 * @return std::tuple<std::vector<PauliNode_ptr>, std::vector<PauliNode_ptr>>
 */
std::tuple<std::vector<PauliNode_ptr>, std::vector<PauliNode_ptr>>
gpg_from_unordered_set(const std::vector<SymPauliTensor>& unordered_set);

/**
 * @brief Converts the given circuit into a GPGraph and conjugates each node
 * by greedily applying 2-qubit Clifford gates until the node can be realised
 * as a single-qubit gate, a measurement, or a reset. The final Clifford
 * operator is synthesized in a similar fashion. Allows early termination
 * from a thread via a stop_flag.
 *
 * @param circ
 * @param stop_flag
 * @param discount_rate
 * @param depth_weight
 * @param max_lookahead
 * @param max_tqe_candidates
 * @param seed
 * @param allow_zzphase
 * @return Circuit
 */
Circuit greedy_pauli_graph_synthesis_flag(
    const Circuit& circ, std::atomic<bool>& stop_flag,
    double discount_rate = 0.7, double depth_weight = 0.3,
    unsigned max_lookahead = 500, unsigned max_tqe_candidates = 500,
    unsigned seed = 0, bool allow_zzphase = false);

/**
 * @brief Converts the given circuit into a GPGraph and conjugates each node
 * by greedily applying 2-qubit Clifford gates until the node can be realised
 * as a single-qubit gate, a measurement, or a reset. The final Clifford
 * operator is synthesized in a similar fashion.
 *
 * @param circ
 * @param discount_rate
 * @param depth_weight
 * @param max_lookahead
 * @param max_tqe_candidates
 * @param seed
 * @param allow_zzphase
 * @return Circuit
 */
Circuit greedy_pauli_graph_synthesis(
    const Circuit& circ, double discount_rate = 0.7, double depth_weight = 0.3,
    unsigned max_lookahead = 500, unsigned max_tqe_candidates = 500,
    unsigned seed = 0, bool allow_zzphase = false);

/**
 * @brief Synthesise a set of unordered Pauli exponentials by applying Clifford
 * gates and single qubit rotations in a greedy fashion. Assume all Pauli
 * strings have the same length.
 *
 * @param unordered_set
 * @param depth_weight
 * @param max_lookahead
 * @param max_tqe_candidates
 * @param seed
 * @param allow_zzphase
 * @return Circuit
 */
Circuit greedy_pauli_set_synthesis(
    const std::vector<SymPauliTensor>& unordered_set, double depth_weight = 0.3,
    unsigned max_lookahead = 500, unsigned max_tqe_candidates = 500,
    unsigned seed = 0, bool allow_zzphase = false);

}  // namespace GreedyPauliSimp

TKET_EXPORT Transform greedy_pauli_optimisation(
    double discount_rate = 0.7, double depth_weight = 0.3,
    unsigned max_lookahead = 500, unsigned max_tqe_candidates = 500,
    unsigned seed = 0, bool allow_zzphase = false,
    unsigned thread_timeout = 100, unsigned trials = 1);

}  // namespace Transforms

}  // namespace tket
