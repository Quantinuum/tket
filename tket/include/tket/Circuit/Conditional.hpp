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

// This file is in the Circuit component because Conditional::deserialize() may
// require deserialization of a Box.

#include "tket/Ops/Op.hpp"
#include "tket_export.h"

namespace tket {

/**
 * Decorates another op, adding a QASM-style classical condition
 */
class TKET_EXPORT Conditional : public Op {
 public:
  /**
   * Construct from a given op, condition width and value
   *
   * @param op op to control
   * @param width number of bits in condition
   * @param value Little-endian value of condition bits required to
   *              execute op (e.g. value 2 (10b) means bits[0] must be 0
   *              and bits[1] must be 1)
   */
  explicit Conditional(const Op_ptr &op, unsigned width, unsigned value);

  /**
   * Copy constructor
   */
  Conditional(const Conditional &other);

  ~Conditional() override;

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  SymSet free_symbols() const override;

  /**
   * Equality check between two Conditional instances
   */
  bool is_equal(const Op &other) const override;

  unsigned n_qubits() const override;

  std::optional<Pauli> commuting_basis(port_t i) const override;

  op_signature_t get_signature() const override;

  nlohmann::json serialize() const override;

  static Op_ptr deserialize(const nlohmann::json &j);

  std::string get_command_str(const unit_vector_t &args) const override;

  Op_ptr dagger() const override;

  Op_ptr get_op() const;

  unsigned get_width() const;

  unsigned get_value() const;

 protected:
  Conditional();

 private:
  const Op_ptr op_;
  const unsigned width_;
  const unsigned value_;
};

}  // namespace tket
