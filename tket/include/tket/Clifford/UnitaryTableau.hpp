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

#include "SymplecticTableau.hpp"

#include "tket_export.h"

namespace tket {

// Forward declare classes for friends
class Circuit;
class UnitaryRevTableau;

class TKET_EXPORT UnitaryTableau {
  /**
   * An implementation of the stabilizer-destabilizer tableau for unitary
   * Cliffords in Aaronson & Gottesman, "Improved Simulation of Stabilizer
   * Circuits", https://arxiv.org/pdf/quant-ph/0406196.pdf
   *
   * If a Pauli is placed at an input before the unitary, there is a Pauli
   * string over the outputs which would have an equivalent effect. The rows
   * correspond to these Pauli strings for X and Z on each input qubit. In other
   * terms, the ith X row is the (phaseful) Pauli string P such that P C X_i =
   * C, and similarly for the Z rows.
   *
   * The Z rows generate the stabilizer group for the state prepared when the
   * unitary is applied to the initial |0>^n state, with the X rows extending
   * this to generate the full n-fold Pauli group.
   *
   * Qubits indexed using Qubit objects.
   */
 public:
  /**
   * Construct the tableau for the identity over n qubits (given default qubit
   * names).
   */
  explicit UnitaryTableau(unsigned n);

  /**
   * Construct the tableau for the identity over specific qubits.
   */
  explicit UnitaryTableau(const qubit_vector_t& qbs);

  /**
   * Construct a tableau from the underlying binary matrices.
   * Qubits given default names.
   * @param xx The X component of the X rows
   * @param xz The Z component of the X rows
   * @param xph The phases of the X rows
   * @param zx The X component of the Z rows
   * @param zz The Z component of the Z rows
   * @param zph The phases of the Z rows
   */
  explicit UnitaryTableau(
      const MatrixXb& xx, const MatrixXb& xz, const VectorXb& xph,
      const MatrixXb& zx, const MatrixXb& zz, const VectorXb& zph);

  /**
   * Other required constructors
   */
  UnitaryTableau();
  UnitaryTableau(const UnitaryTableau& other) = default;
  UnitaryTableau(UnitaryTableau&& other) = default;
  UnitaryTableau& operator=(const UnitaryTableau& other) = default;
  UnitaryTableau& operator=(UnitaryTableau&& other) = default;

  /**
   * Read off an X row as a Pauli string
   */
  SpPauliStabiliser get_xrow(const Qubit& qb) const;

  /**
   * Read off a Z row as a Pauli string
   */
  SpPauliStabiliser get_zrow(const Qubit& qb) const;

  /**
   * Combine rows into a single row according to a SpPauliStabiliser
   */
  SpPauliStabiliser get_row_product(const SpPauliStabiliser& qpt) const;

  /**
   * Access all IDs for the qubits used in the tableau.
   */
  std::set<Qubit> get_qubits() const;

  /**
   * Transform the tableau according to consuming a Clifford gate at either end
   * of the circuit.
   */
  void apply_S_at_front(const Qubit& qb);
  void apply_S_at_end(const Qubit& qb);
  void apply_Z_at_front(const Qubit& qb);
  void apply_Z_at_end(const Qubit& qb);
  void apply_V_at_front(const Qubit& qb);
  void apply_V_at_end(const Qubit& qb);
  void apply_X_at_front(const Qubit& qb);
  void apply_X_at_end(const Qubit& qb);
  void apply_H_at_front(const Qubit& qb);
  void apply_H_at_end(const Qubit& qb);
  void apply_CX_at_front(const Qubit& control, const Qubit& target);
  void apply_CX_at_end(const Qubit& control, const Qubit& target);
  void apply_gate_at_front(OpType type, const qubit_vector_t& qbs);
  void apply_gate_at_end(OpType type, const qubit_vector_t& qbs);

  /**
   * Transform the tableau according to consuming a Clifford-phase Pauli gadget
   * at either end of the circuit.
   *
   * @param pauli The string of the Pauli gadget
   * @param half_pis The Clifford angle: {0, 1, 2, 3} represents {0, pi/2, pi,
   * -pi/2}
   */
  void apply_pauli_at_front(const SpPauliStabiliser& pauli, unsigned half_pis);
  void apply_pauli_at_end(const SpPauliStabiliser& pauli, unsigned half_pis);

  /**
   * Combine two tableaux in sequence.
   * Will throw an exception if the tableaux are not over the same set of
   * qubits.
   *
   * @param first first circuit
   * @param second second circuit
   *
   * @return The tableau corresponding to applying \p first, followed by \p
   * second
   */
  static UnitaryTableau compose(
      const UnitaryTableau& first, const UnitaryTableau& second);

  /**
   * Gives the UnitaryTableau corresponding to the inverse (dagger) or transpose
   * unitary. This is distinct from simply transposing the binary matrix
   * representation. Takes time O(N^3) for N qubits.
   */
  UnitaryTableau dagger() const;
  UnitaryTableau transpose() const;

  /**
   * Gives the UnitaryTableau corresponding to the complex conjugate unitary.
   * This calls conjugate() on the underlying SymplecticTableau.
   */
  UnitaryTableau conjugate() const;

  friend TKET_EXPORT UnitaryTableau circuit_to_unitary_tableau(const Circuit& circ);
  friend TKET_EXPORT Circuit unitary_tableau_to_circuit(const UnitaryTableau& tab);

  friend void to_json(nlohmann::json& j, const UnitaryTableau& tab);
  friend void from_json(const nlohmann::json& j, UnitaryTableau& tab);

  friend std::ostream& operator<<(std::ostream& os, const UnitaryTableau& tab);
  friend std::ostream& operator<<(
      std::ostream& os, const UnitaryRevTableau& tab);
  bool operator==(const UnitaryTableau& other) const;

 private:
  /**
   * The actual binary tableau.
   * Rows 0-(n-1) are the X rows, n-(2n-1) are the Z rows.
   */
  SymplecticTableau tab_;

  /** Map from qubit IDs to their row/column index in tableau */
  boost::bimap<Qubit, unsigned> qubits_;
};

JSON_DECL(UnitaryTableau)

std::ostream& operator<<(std::ostream& os, const UnitaryTableau& tab);

class TKET_EXPORT UnitaryRevTableau {
  /**
   * Whereas UnitaryTableau rows are over the output segment with fixed single
   * paulis over the input segment, UnitaryRevTableau flips this around with
   * each row representing a Pauli string over the input segment and a fixed
   * single pauli over the output segment. That is, the ith X row is the
   * (phaseful) Pauli string P such that X_i C P = C, and similarly for the Z
   * rows.
   *
   * This is maintained by storing a UnitaryTableau for the inverse unitary
   * C^dagger. Since each Pauli string is Hermitian, X_i C P = C is encoded as
   * C^dagger = (X_i C P)^dagger = P C^dagger X_i, so no need to modify rows on
   * reading them, but when applying gates this class will invert the gate (or
   * reverse the order of gate application) before applying to the inner
   * UnitaryTableau.
   */
 public:
  /**
   * Construct the tableau for the identity over n qubits (given default qubit
   * names).
   */
  explicit UnitaryRevTableau(unsigned n);

  /**
   * Construct the tableau for the identity over specific qubits.
   */
  explicit UnitaryRevTableau(const qubit_vector_t& qbs);

  /**
   * Construct a tableau from the underlying binary matrices.
   * Qubits given default names.
   */
  explicit UnitaryRevTableau(
      const MatrixXb& xx, const MatrixXb& xz, const VectorXb& xph,
      const MatrixXb& zx, const MatrixXb& zz, const VectorXb& zph);

  /**
   * Other required constructors
   */
  UnitaryRevTableau();
  UnitaryRevTableau(const UnitaryRevTableau& other) = default;
  UnitaryRevTableau(UnitaryRevTableau&& other) = default;
  UnitaryRevTableau& operator=(const UnitaryRevTableau& other) = default;
  UnitaryRevTableau& operator=(UnitaryRevTableau&& other) = default;

  /**
   * Read off an X row as a Pauli string
   */
  SpPauliStabiliser get_xrow(const Qubit& qb) const;

  /**
   * Read off a Z row as a Pauli string
   */
  SpPauliStabiliser get_zrow(const Qubit& qb) const;

  /**
   * Combine rows into a single row according to a SpPauliStabiliser
   */
  SpPauliStabiliser get_row_product(const SpPauliStabiliser& qpt) const;

  /**
   * Access all IDs for the qubits used in the tableau.
   */
  std::set<Qubit> get_qubits() const;

  /**
   * Transform the tableau according to consuming a Clifford gate at either end
   * of the circuit.
   */
  void apply_S_at_front(const Qubit& qb);
  void apply_S_at_end(const Qubit& qb);
  void apply_Z_at_front(const Qubit& qb);
  void apply_Z_at_end(const Qubit& qb);
  void apply_V_at_front(const Qubit& qb);
  void apply_V_at_end(const Qubit& qb);
  void apply_X_at_front(const Qubit& qb);
  void apply_X_at_end(const Qubit& qb);
  void apply_H_at_front(const Qubit& qb);
  void apply_H_at_end(const Qubit& qb);
  void apply_CX_at_front(const Qubit& control, const Qubit& target);
  void apply_CX_at_end(const Qubit& control, const Qubit& target);
  void apply_gate_at_front(OpType type, const qubit_vector_t& qbs);
  void apply_gate_at_end(OpType type, const qubit_vector_t& qbs);

  /**
   * Transform the tableau according to consuming a Clifford-phase Pauli gadget
   * at either end of the circuit.
   *
   * @param pauli The string of the Pauli gadget
   * @param half_pis The Clifford angle: {0, 1, 2, 3} represents {0, pi/2, pi,
   * -pi/2}
   */
  void apply_pauli_at_front(const SpPauliStabiliser& pauli, unsigned half_pis);
  void apply_pauli_at_end(const SpPauliStabiliser& pauli, unsigned half_pis);

  /**
   * Combine two tableaux in sequence.
   * Will throw an exception if the tableaux are not over the same set of
   * qubits.
   *
   * @param first first circuit
   * @param second second circuit
   *
   * @return The tableau corresponding to applying \p first, followed by \p
   * second
   */
  static UnitaryRevTableau compose(
      const UnitaryRevTableau& first, const UnitaryRevTableau& second);

  /**
   * Gives the UnitaryRevTableau corresponding to the inverse (dagger) or
   * transpose unitary. This is distinct from simply transposing the binary
   * matrix representation. Takes time O(N^3) for N qubits.
   */
  UnitaryRevTableau dagger() const;
  UnitaryRevTableau transpose() const;

  /**
   * Gives the UnitaryRevTableau corresponding to the complex conjugate unitary.
   * This calls conjugate() on the underlying SymplecticTableau.
   */
  UnitaryRevTableau conjugate() const;

  friend TKET_EXPORT UnitaryRevTableau circuit_to_unitary_rev_tableau(const Circuit& circ);
  friend TKET_EXPORT Circuit unitary_rev_tableau_to_circuit(const UnitaryRevTableau& tab);

  friend void to_json(nlohmann::json& j, const UnitaryRevTableau& tab);
  friend void from_json(const nlohmann::json& j, UnitaryRevTableau& tab);

  friend std::ostream& operator<<(
      std::ostream& os, const UnitaryRevTableau& tab);
  bool operator==(const UnitaryRevTableau& other) const;

 private:
  UnitaryTableau tab_;
};

JSON_DECL(UnitaryRevTableau)

std::ostream& operator<<(std::ostream& os, const UnitaryRevTableau& tab);

}  // namespace tket
