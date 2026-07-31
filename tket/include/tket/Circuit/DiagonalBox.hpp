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

#include <memory>

#include "Boxes.hpp"

#include "tket_export.h"

namespace tket {
/**
 * Box to synthesise a diagonal operator
 */
class TKET_EXPORT DiagonalBox : public Box {
 public:
  /**
   * Construct a circuit that synthesise the given unitary diagonal operator
   *
   * @param diagonal the diagonal entries of the operator
   * @param upper_triangle the diagonal operator will be decomposed as a
   * sequence of multiplexed-Rz gates. This argument decides whether the
   * multiplexed-Rz gates take the shape of an upper triangle or a lower
   * triangle.
   */
  explicit DiagonalBox(
      const Eigen::VectorXcd &diagonal, bool upper_triangle = true);

  /**
   * Copy constructor
   */
  DiagonalBox(const DiagonalBox &other);
  ~DiagonalBox() override {}

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &) const override {
    return std::make_shared<DiagonalBox>(*this);
  }

  SymSet free_symbols() const override { return {}; }

  /**
   * Equality check between two DiagonalBox instances
   */
  bool is_equal(const Op &op_other) const override;

  Op_ptr dagger() const override;
  Op_ptr transpose() const override;

  op_signature_t get_signature() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

  Eigen::VectorXcd get_diagonal() const;
  bool is_upper_triangle() const;

 protected:
  /**
   * @brief Generate the decomposed circuit using
   * multiplexed-Rz gates
   *
   */
  void generate_circuit() const override;

  DiagonalBox()
      : Box(OpType::DiagonalBox), diagonal_(), upper_triangle_(true) {}

 private:
  const Eigen::VectorXcd diagonal_;
  const bool upper_triangle_;
};
}  // namespace tket
