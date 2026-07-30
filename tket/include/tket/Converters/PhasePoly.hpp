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
#include "tket/Circuit/Boxes.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Utils/MatrixAnalysis.hpp"
#include "tket_export.h"

namespace tket {

/**
 * PhasePolynomial is just a sequence of parities: that is, terms of the form
 * \f$ e^{\alpha \bigotimes Z} \f$, where Z is a Pauli Z. This is capable of
 * representing a restricted set of circuits made up of CNOTs and Rzs:
 * specifically, circuits where the output qubits have the same state as the
 * inputs, modulo (local) phases. The vectors are always assumed to be the same
 * size as the qubit count. */
typedef std::map<std::vector<bool>, Expr> PhasePolynomial;
typedef std::pair<std::vector<bool>, Expr> phase_term_t;

/* arXiv:1712.01859 : heuristic for synthesis of phase polynomial into
a CNOT-dihedral circuit (ie CNOT + Rz). Architecture-blind. */
Circuit gray_synth(
    unsigned n_qubits, const std::list<phase_term_t> &parities,
    const MatrixXb &linear_transformation);

/**
 * A PhasePolyBox is capable of representing arbitrary Circuits made up of CNOT
 * and RZ, as a PhasePolynomial plus a boolean matrix representing an additional
 * linear transformation.
 */
class TKET_EXPORT PhasePolyBox : public Box {
 public:
  explicit PhasePolyBox(const Circuit &circ);
  explicit PhasePolyBox(
      unsigned n_qubits, const boost::bimap<Qubit, unsigned> &qubit_indices,
      const PhasePolynomial &phase_polynomial,
      const MatrixXb &linear_transformation);

  PhasePolyBox() : Box(OpType::PhasePolyBox) {}

  /**
   * Copy constructor
   */
  PhasePolyBox(const PhasePolyBox &other);

  ~PhasePolyBox() override {}

  bool is_clifford() const override;

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  SymSet free_symbols() const override;

  bool is_equal(const Op &op_other) const override {
    const PhasePolyBox &other = dynamic_cast<const PhasePolyBox &>(op_other);
    return (
        this->n_qubits_ == other.n_qubits_ &&
        this->phase_polynomial_ == other.phase_polynomial_ &&
        this->linear_transformation_ == other.linear_transformation_ &&
        this->qubit_indices_ == other.qubit_indices_);
  }

  const PhasePolynomial &get_phase_polynomial() const {
    return phase_polynomial_;
  }
  const MatrixXb &get_linear_transformation() const {
    return linear_transformation_;
  }
  const boost::bimap<Qubit, unsigned> &get_qubit_indices() const {
    return qubit_indices_;
  }

  unsigned get_n_qubits() const { return n_qubits_; }

  static Op_ptr from_json(const nlohmann::json &j);

  Circuit generate_circuit_with_original_placement() const;

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  // automatically uses GraySynth
  // (https://arxiv.org/pdf/1712.01859.pdf)
  // other circuit generation methods (for architectures) to come!
  void generate_circuit() const override;

 private:
  unsigned n_qubits_;
  boost::bimap<Qubit, unsigned> qubit_indices_;
  PhasePolynomial phase_polynomial_;
  MatrixXb linear_transformation_;
};

/**
 * this class realises the conversion all sub circuits of a given circuits which
 *  contains only CX+Rz to a PhasePolyBox. The circuit should contain only
 *  CX, Rz, H, measure, reset, collapse, barrier.
 */
class CircToPhasePolyConversion {
 public:
  /**
   * converts all sub circuits of a given circuits which contains only
   * CX+Rz to a PhasePolyBox.
   * @throw not implemented for unsupported gates
   * @param circ circuit to be converted
   * @param min_size value for the minimal number of CX in each box, groups with
   * less than min_size CX gates are not converted to a PhasePolyBox, default
   * value is 0
   */
  explicit CircToPhasePolyConversion(
      const Circuit &circ, unsigned min_size = 0);
  void convert();
  Circuit get_circuit() const;

 private:
  enum class QubitType { pre, in, post };
  void add_phase_poly_box();
  unsigned nq_;
  unsigned nb_;
  unsigned min_size_;
  unsigned box_size_;
  std::map<Qubit, unsigned> qubit_indices_;
  std::map<Bit, unsigned> bit_indices_;
  std::vector<QubitType> qubit_types_;
  qubit_vector_t all_qu_;
  Circuit input_circ_;
  Circuit box_circ_;
  Circuit post_circ_;
  Circuit circ_;
  Circuit empty_circ_;
};

}  // namespace tket
