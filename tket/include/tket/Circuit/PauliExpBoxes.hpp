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

#include "Boxes.hpp"
#include "tket/Diagonalisation/PauliPartition.hpp"
#include "tket/Transformations/PauliOptimisation.hpp"

#include "tket_export.h"

namespace tket {

class PauliExpBoxInvalidity : public std::logic_error {
 public:
  explicit PauliExpBoxInvalidity(const std::string &message)
      : std::logic_error(message) {}
};

/**
 * Operation defined as the exponential of a tensor of Pauli operators
 */
class TKET_EXPORT PauliExpBox : public Box {
 public:
  /**
   * The operation implements the unitary operator
   * \f$ e^{-\frac12 i \pi t \sigma_0 \otimes \sigma_1 \otimes \cdots} \f$
   * where \f$ \sigma_i \in \{I,X,Y,Z\} \f$ are the Pauli operators and \f$ t
   * \f$ is the coefficient.
   */
  PauliExpBox(
      const SymPauliTensor &paulis,
      CXConfigType cx_config_type = CXConfigType::Tree);

  /**
   * Construct from the empty vector
   */
  PauliExpBox();

  /**
   * Copy constructor
   */
  PauliExpBox(const PauliExpBox &other);

  ~PauliExpBox() override = default;

  bool is_clifford() const override;

  SymSet free_symbols() const override;

  /**
   * Equality check between two PauliExpBox instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get the Pauli string */
  std::vector<Pauli> get_paulis() const { return paulis_.string; }

  /** Get the phase parameter */
  Expr get_phase() const { return paulis_.coeff; }

  /** Get the cx_config parameter (affects box decomposition) */
  CXConfigType get_cx_config() const { return cx_config_; }

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

 private:
  SymPauliTensor paulis_;
  CXConfigType cx_config_;
};

class TKET_EXPORT PauliExpPairBox : public Box {
 public:
  PauliExpPairBox(
      const SymPauliTensor &paulis0, const SymPauliTensor &paulis1,
      CXConfigType cx_config_type = CXConfigType::Tree);

  /**
   * Construct from the empty vector
   */
  PauliExpPairBox();

  /**
   * Copy constructor
   */
  PauliExpPairBox(const PauliExpPairBox &other);

  ~PauliExpPairBox() override {}

  bool is_clifford() const override;

  SymSet free_symbols() const override;

  /**
   * Equality check between two instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get Pauli strings for the pair */
  std::pair<std::vector<Pauli>, std::vector<Pauli>> get_paulis_pair() const {
    return std::make_pair(paulis0_.string, paulis1_.string);
  }

  /** Get phase parameters for the pair */
  std::pair<Expr, Expr> get_phase_pair() const {
    return std::make_pair(paulis0_.coeff, paulis1_.coeff);
  }

  /** Get the cx_config parameter (affects box decomposition) */
  CXConfigType get_cx_config() const { return cx_config_; }

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

 private:
  SymPauliTensor paulis0_;
  SymPauliTensor paulis1_;
  CXConfigType cx_config_;
};

class TKET_EXPORT PauliExpCommutingSetBox : public Box {
 public:
  PauliExpCommutingSetBox(
      const std::vector<SymPauliTensor> &pauli_gadgets,
      CXConfigType cx_config_type = CXConfigType::Tree);

  /**
   * Construct from the empty vector
   */
  PauliExpCommutingSetBox();

  /**
   * Copy constructor
   */
  PauliExpCommutingSetBox(const PauliExpCommutingSetBox &other);

  ~PauliExpCommutingSetBox() override {}

  bool paulis_commute() const;

  bool is_clifford() const override;

  SymSet free_symbols() const override;

  /**
   * Equality check between two instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get the pauli gadgets */
  auto get_pauli_gadgets() const { return pauli_gadgets_; }

  /** Get the cx_config parameter (affects box decomposition) */
  auto get_cx_config() const { return cx_config_; }

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

 private:
  std::vector<SymPauliTensor> pauli_gadgets_;
  CXConfigType cx_config_;
};

class TKET_EXPORT TermSequenceBox : public Box {
 public:
  TermSequenceBox(
      const std::vector<SymPauliTensor> &pauli_gadgets,
      Transforms::PauliSynthStrat synth_strategy =
          Transforms::PauliSynthStrat::Sets,
      PauliPartitionStrat partition_strategy =
          PauliPartitionStrat::CommutingSets,
      GraphColourMethod graph_colouring = GraphColourMethod::Lazy,
      CXConfigType cx_configuration = CXConfigType::Tree,
      double depth_weight = 0.3);

  /**
   * Construct from the empty vector
   */
  TermSequenceBox();

  /**
   * Copy constructor
   */
  TermSequenceBox(const TermSequenceBox &other);

  ~TermSequenceBox() override {}

  bool is_clifford() const override;

  SymSet free_symbols() const override;

  /**
   * Equality check between two instances
   */
  bool is_equal(const Op &op_other) const override;

  /** Get the circuit synthesis strategy parameter (affects box decomposition)
   */
  Transforms::PauliSynthStrat get_synth_strategy() const;

  /** Get the pauli partitioning strategy parameter (affects box decomposition)
   */
  PauliPartitionStrat get_partition_strategy() const;

  /** Get the graph colouring parameter (affects box decomposition) */
  GraphColourMethod get_graph_colouring() const;

  /** Get the pauli gadgets */
  std::vector<SymPauliTensor> get_pauli_gadgets() const;

  /** Get the cx config parameter (affects box decomposition) */
  CXConfigType get_cx_config() const;

  /** Tuning parameter for depth optimisation, only applies to
   * PauliPartitionStrat::Greedy */
  double get_depth_weight() const;

  Op_ptr dagger() const override;

  Op_ptr transpose() const override;

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

 private:
  std::vector<SymPauliTensor> pauli_gadgets_;
  Transforms::PauliSynthStrat synth_strategy_;
  PauliPartitionStrat partition_strategy_;
  GraphColourMethod graph_colouring_;
  CXConfigType cx_configuration_;
  double depth_weight_;
};

/**
 * Constructs a PauliExpBox for a single pauli gadget and appends it to a
 * circuit.
 *
 * @param circ The circuit to append the box to
 * @param pauli The pauli operator of the gadget; coefficient gives the rotation
 * angle in half-turns
 * @param cx_config The CX configuration to be used during synthesis
 */
void append_single_pauli_gadget_as_pauli_exp_box(
    Circuit &circ, const SpSymPauliTensor &pauli, CXConfigType cx_config);

/**
 * Constructs a PauliExpPairBox for a pair of pauli gadgets and appends it to a
 * circuit. The pauli gadgets may or may not commute, so the ordering matters.
 *
 * @param circ The circuit to append the box to
 * @param pauli0 The pauli operator of the first gadget; coefficient gives the
 * rotation angle in half-turns
 * @param pauli1 The pauli operator of the second gadget; coefficient gives the
 * rotation angle in half-turns
 * @param cx_config The CX configuration to be used during synthesis
 */
void append_pauli_gadget_pair_as_box(
    Circuit &circ, const SpSymPauliTensor &pauli0,
    const SpSymPauliTensor &pauli1, CXConfigType cx_config);

/**
 * Constructs a PauliExpCommutingSetBox for a set of mutually commuting pauli
 * gadgets and appends it to a circuit. As the pauli gadgets all commute, the
 * ordering does not matter semantically, but may yield different synthesised
 * circuits.
 *
 * @param circ The circuit to append the box to
 * @param gadgets Description of the pauli gadgets; coefficients give the
 * rotation angles in half-turns
 * @param cx_config The CX configuration to be used during synthesis
 */
void append_commuting_pauli_gadget_set_as_box(
    Circuit &circ, const std::list<SpSymPauliTensor> &gadgets,
    CXConfigType cx_config);

}  // namespace tket
