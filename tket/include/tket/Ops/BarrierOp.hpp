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

#include "Op.hpp"

#include "tket_export.h"

namespace tket {

class TKET_EXPORT BarrierOp : public Op {
 public:
  explicit BarrierOp(
      op_signature_t signature = {}, const std::string &_data = "");

  Op_ptr symbol_substitution(
      const SymEngine::map_basic_basic &sub_map) const override;

  SymSet free_symbols() const override;

  unsigned n_qubits() const override;

  op_signature_t get_signature() const override;

  std::string get_data() const { return data_; }

  bool is_clifford() const override;

  ~BarrierOp() override;

  /**
   * Equality check between two BarrierOp instances
   */
  bool is_equal(const Op &other) const override;

  nlohmann::json serialize() const override;

  static Op_ptr deserialize(const nlohmann::json &j);

 private:
  op_signature_t signature_; /**< Types of inputs */
  /**
   * additional data given by the user, can be passed on to backend
   */
  const std::string data_;
};

}  // namespace tket
