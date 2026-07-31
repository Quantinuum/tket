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

#include <optional>
#include <string>

#include "OpType.hpp"
#include "OpTypeInfo.hpp"

#include "tket_export.h"

namespace tket {

/** Optional unsigned integer */
typedef std::optional<unsigned> OptUInt;

/** Unspecified unsigned integer */
inline const OptUInt any = std::nullopt;

/**
 * Operation descriptor
 *
 * An object of this class holds information about a specific operation type.
 */
class TKET_EXPORT OpDesc {
 public:
  explicit OpDesc(OpType type);

  /** Type of operation */
  OpType type() const;

  /** Name */
  std::string name() const;

  /** Name in Latex representation */
  std::string latex() const;

  /** Number of phase parameters */
  unsigned n_params() const;

  /** Types of each input/output */
  std::optional<op_signature_t> signature() const;

  /** Number of input and output qubits */
  OptUInt n_qubits() const;

  /** Number of classical bits read */
  OptUInt n_boolean() const;

  /** Number of classical bits written to */
  OptUInt n_classical() const;

  /** Whether the 'operation' is actually an input or output */
  bool is_meta() const;

  /** Whether the operation is a Barrier */
  bool is_barrier() const;

  /** Whether the operation is a box of some kind */
  bool is_box() const;

  /** Whether the operation is a normal (quantum or classical) gate */
  bool is_gate() const;

  /** Whether the operation is for control flow */
  bool is_flowop() const;

  /** Whether the operation is purely classical */
  bool is_classical() const;

  /**
   * Whether this is a parametrized rotation.
   *
   * For our purposes, an operation \f$ O \f$ is a rotation if and only if it
   * has a single real-valued parameter which is additive in the sense that
   * \f$ O(\alpha) \circ O(\beta) = O(\alpha + \beta) \f$.
   */
  bool is_rotation() const;

  /**
   * The modulus for a parameter
   *
   * @param i the index of the parameter
   *
   * @return \f$ n \f$ such that the parameter is to be regarded as an element
   * of \f$ \mathbb{R} / n \mathbb{Z} \f$.
   */
  unsigned param_mod(unsigned i) const;

  /** Whether the operation has no defined dagger */
  bool is_oneway() const;

  /** Whether the operation is a single-qubit unitary */
  bool is_singleq_unitary() const;

  /** Whether the operation is a Clifford gate */
  bool is_clifford_gate() const;

  /** Whether the operation is a parameterised Pauli rotation */
  bool is_parameterised_pauli_rotation() const;

 private:
  const OpType type_;
  const OpTypeInfo info_;
  const bool is_meta_;
  const bool is_barrier_;
  const bool is_box_;
  const bool is_gate_;
  const bool is_flowop_;
  const bool is_classical_;
  const bool is_rotation_;
  const bool is_oneway_;
  const bool is_clifford_;
  const bool is_parameterised_pauli_rotation_;
};

}  // namespace tket
