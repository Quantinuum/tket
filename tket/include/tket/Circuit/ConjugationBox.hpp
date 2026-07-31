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

#include "tket_export.h"

namespace tket {
/**
 * Box to express computations that follow the compute-action-uncompute pattern
 */
class TKET_EXPORT ConjugationBox : public Box {
 public:
  /**
   * @brief Construct a new ConjugationBox object from operations that perform
   * compute, action, and uncompute. All three operations need to have the same
   * signature.
   *
   * @param compute the compute operation
   * @param action the action operation
   * @param uncompute optional uncompute operation, default to compute.dagger().
   * If provided, the user needs to make sure that uncompute.dagger() and
   * compute have the same unitary.
   */
  explicit ConjugationBox(
      const Op_ptr &compute, const Op_ptr &action,
      const std::optional<Op_ptr> uncompute = std::nullopt);

  /**
   * Copy constructor
   */
  ConjugationBox(const ConjugationBox &other);
  ~ConjugationBox() override {}

  Op_ptr symbol_substitution(const SymEngine::map_basic_basic &) const override;

  SymSet free_symbols() const override;

  /**
   * Equality check between two ConjugationBox instances
   */
  bool is_equal(const Op &op_other) const override;

  Op_ptr dagger() const override;
  Op_ptr transpose() const override;

  Op_ptr get_compute() const { return compute_; }
  Op_ptr get_action() const { return action_; }
  std::optional<Op_ptr> get_uncompute() const { return uncompute_; }

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

 protected:
  void generate_circuit() const override;

  ConjugationBox()
      : Box(OpType::ConjugationBox), compute_(), action_(), uncompute_() {}

 private:
  const Op_ptr compute_;
  const Op_ptr action_;
  const std::optional<Op_ptr> uncompute_;
};
}  // namespace tket
