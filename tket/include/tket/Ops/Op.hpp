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

/**
 * @file
 * @brief Operations
 */

#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "OpPtr.hpp"
#include "tket/OpType/OpDesc.hpp"
#include "tket/OpType/OpTypeInfo.hpp"
#include "tket/Utils/EigenConfig.hpp"
#include "tket/Utils/Expression.hpp"
#include "tket/Utils/Json.hpp"
#include "tket/Utils/PauliTensor.hpp"
#include "tket/Utils/UnitID.hpp"
#include "tket_export.h"

namespace tket {

/** Wrong number of parameters for an operation */
class InvalidParameterCount : public std::logic_error {
 public:
  InvalidParameterCount()
      : std::logic_error("Gate has an invalid number of parameters") {}
};

/** A specific entry or exit port of a vertex */
typedef unsigned port_t;

/**
 * Abstract class representing an operation type
 */
class TKET_EXPORT Op : public std::enable_shared_from_this<Op> {
 public:
  /**
   * Inverse (of a unitary operation)
   *
   * @throw BadOpType if operation is not unitary
   */
  virtual Op_ptr dagger() const { throw BadOpType(get_type()); }

  /**
   * Transpose of a unitary operation
   */
  virtual Op_ptr transpose() const { throw BadOpType(get_type()); };

  /**
   * Operation with values for symbols substituted
   *
   * @param sub_map map from symbols to values
   *
   * @return New operation with symbols substituted, or a null pointer if
   *         the operation type does not support symbols.
   */
  virtual Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const = 0;

  /** Sequence of phase parameters, if applicable */
  virtual std::vector<Expr> get_params() const { throw BadOpType(get_type()); }

  /** Sequence of phase parameters reduced to canonical range, if applicable */
  virtual std::vector<Expr> get_params_reduced() const {
    throw BadOpType(get_type());
  }

  /* Number of qubits */
  virtual unsigned n_qubits() const { throw BadOpType(get_type()); }

  /** String representation */
  virtual std::string get_name(bool latex = false) const;

  /** Command representation */
  virtual std::string get_command_str(const unit_vector_t &args) const;

  /** Get operation descriptor */
  OpDesc get_desc() const { return desc_; }

  /** Get operation type */
  OpType get_type() const { return type_; }

  /** Set of all free symbols occurring in operation parameters. */
  virtual SymSet free_symbols() const = 0;

  /**
   * Which Pauli, if any, commutes with the operation at a given port
   *
   * @param i port at which Pauli should commute
   * @return A Pauli that commutes with the given operation
   * @retval std::nullopt no Pauli commutes (or operation not a gate)
   * @retval Pauli::I any Pauli commutes
   */
  virtual std::optional<Pauli> commuting_basis(port_t i) const {
    (void)i;
    return std::nullopt;
  }

  /**
   * Whether the operation commutes with the given Pauli at the given port
   *
   * @param colour Pauli operation type
   * @param i port of the operation
   */
  virtual bool commutes_with_basis(
      const std::optional<Pauli> &colour, port_t i) const {
    const std::optional<Pauli> my_colour = commuting_basis(i);
    if (!colour || !my_colour) {
      // Nothing can commute with std::nullopt
      return false;
    } else {
      return colour == Pauli::I || my_colour == Pauli::I || colour == my_colour;
    }
  }

  /**
   * return if the op is external
   */
  virtual bool is_extern() const { return false; }

  /** Vector specifying type of data for each port on op */
  virtual op_signature_t get_signature() const = 0;

  /**
   * Test whether operation is identity up to a phase and return phase if so.
   *
   * @return phase, as multiple of pi, if operation is identity up to phase
   * @throw BadOpType if operation is not a \ref Gate
   */
  virtual std::optional<double> is_identity() const {
    throw BadOpType(get_type());
  }

  /**
   * Test whether operation is in the Clifford group.
   *
   * A return value of true guarantees that the operation is Clifford. (Note
   * that the converse is not the case: some Clifford operations may not be
   * detected as such.)
   *
   * @retval true operation is in the Clifford group
   */
  virtual bool is_clifford() const { return false; }

  /**
   * Checks for symmetry of port pairs
   *
   * A return value of true means that exchanging these
   * ports does not change effect of operation
   *
   * @retval true operator is invariant under exchange of ports
   *
   * */
  virtual bool has_symmetry(unsigned port1, unsigned port2) const {
    return port1 == port2;
  };

  /**
   * If meaningful and implemented, return the numerical unitary matrix
   * (in ILO-BE convention) which this Op represents.
   *
   * @pre No symbolic parameters.
   * @return unitary matrix (ILO-BE) which this Op represents
   * @throw BadOpType upon error.
   */
  virtual Eigen::MatrixXcd get_unitary() const { throw BadOpType(get_type()); }

  virtual nlohmann::json serialize() const {
    throw JsonError("JSON serialization not yet implemented for " + get_name());
  }

  virtual ~Op() {}

  bool operator==(const Op &other) const {
    return type_ == other.type_ && is_equal(other);
  }

  /**
   * Checks equality between two instances of the same class.
   * The Op object passed as parameter must always be of the same type as this.
   *
   * For base class Op, it is sufficient that they have same type
   */
  virtual bool is_equal(const Op &) const { return true; }

 protected:
  explicit Op(const OpType &type) : desc_(type), type_(type) {}
  const OpDesc desc_; /**< Operation descriptor */
  const OpType type_; /**< Operation type */
};

// friend to stream op (print)
std::ostream &operator<<(std::ostream &os, Op const &operation);

}  // namespace tket
