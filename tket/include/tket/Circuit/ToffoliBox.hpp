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
 * @brief Strategies for synthesising ToffoliBoxes
 * Matching: use multiplexors to perform parallel swaps on hypercubes
 * Cycle: use CnX gates to perform transpositions
 */
enum class ToffoliBoxSynthStrat { Matching, Cycle };

NLOHMANN_JSON_SERIALIZE_ENUM(
    ToffoliBoxSynthStrat, {{ToffoliBoxSynthStrat::Matching, "Matching"},
                           {ToffoliBoxSynthStrat::Cycle, "Cycle"}});

/**
 * @brief Map bitstrings to bitstrings
 *
 */
typedef std::map<std::vector<bool>, std::vector<bool>> state_perm_t;

/**
 * Box to synthesise a state permutation
 */
class TKET_EXPORT ToffoliBox : public Box {
 public:
  /**
   * @brief Construct a circuit that synthesise the given state permutation
   *
   * @param permutation map between basis states
   * @param strat synthesis strategy
   * @param rotation_axis the rotation axis of the multiplexors used in the
   * decomposition. Can be either Rx or Ry. Only applicable to the
   * ToffoliBoxSynthStrat::Matching strategy
   *
   */
  explicit ToffoliBox(
      const state_perm_t &permutation,
      const ToffoliBoxSynthStrat &strat = ToffoliBoxSynthStrat::Matching,
      const OpType &rotation_axis = OpType::Ry);

  /**
   * Copy constructor
   */
  ToffoliBox(const ToffoliBox &other);
  ~ToffoliBox() override {}

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &) const override {
    return std::make_shared<ToffoliBox>(*this);
  }

  SymSet free_symbols() const override { return {}; }

  /**
   * Equality check between two ToffoliBox instances
   */
  bool is_equal(const Op &op_other) const override;

  std::optional<Eigen::MatrixXcd> get_box_unitary() const override;

  Op_ptr dagger() const override;
  Op_ptr transpose() const override;

  op_signature_t get_signature() const override;

  static Op_ptr from_json(const nlohmann::json &j);

  static nlohmann::json to_json(const Op_ptr &op);

  state_perm_t get_permutation() const;
  OpType get_rotation_axis() const;
  ToffoliBoxSynthStrat get_strat() const;

 protected:
  /**
   * @brief Generate the decomposed circuit using
   * a sequence of multiplexed-Rx/Ry gates and a diagonal box
   *
   */
  void generate_circuit() const override;

  ToffoliBox()
      : Box(OpType::ToffoliBox),
        n_(0),
        pow2n_(1),
        permutation_(),
        strat_(ToffoliBoxSynthStrat::Matching),
        rotation_axis_(OpType::Ry) {}

 private:
  const unsigned n_;
  const unsigned pow2n_;
  const state_perm_t permutation_;
  const ToffoliBoxSynthStrat strat_;
  const OpType rotation_axis_;
};
}  // namespace tket
