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
#include "ResourceData.hpp"
#include "tket_export.h"

namespace tket {

/**
 * Exception indicating that dummy boxes cannot be decomposed.
 */
class DummyBoxNotDecomposable : public std::logic_error {
 public:
  DummyBoxNotDecomposable()
      : std::logic_error("Cannot generate circuit from DummyBox") {}
};

/**
 * @brief A placeholder operation that holds resource data.
 *
 * This box type cannot be decomposed into a circuit. It only serves to record
 * resource data for a region of a circuit: for example, upper and lower bounds
 * on gate counts and depth. A circuit containing such a box cannot be executed.
 */
class TKET_EXPORT DummyBox : public Box {
 public:
  /**
   * @brief Construct a new instance from some resource data.
   *
   * @param n_qubits number of qubits
   * @param n_bits number of bits
   * @param resource_data_ resource data
   */
  DummyBox(
      unsigned n_qubits, unsigned n_bits, const ResourceData &resource_data_);

  /**
   * Copy constructor
   */
  DummyBox(const DummyBox &other);

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &) const override {
    return std::make_shared<DummyBox>(*this);
  }

  SymSet free_symbols() const override { return {}; }

  bool is_equal(const Op &op_other) const override;

  unsigned get_n_qubits() const;
  unsigned get_n_bits() const;
  ResourceData get_resource_data() const;

  op_signature_t get_signature() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  /**
   * @brief Throw an exception.
   *
   * This box does not correspond to any actual circuit.
   *
   * @throws DummyBoxNotDecomposable
   */
  void generate_circuit() const override;

 private:
  const unsigned n_qubits;
  const unsigned n_bits;
  const ResourceData resource_data;
};

}  // namespace tket
