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

#include <map>
#include <optional>
#include <string>

#include "EdgeType.hpp"
#include "OpType.hpp"
#include "tket_export.h"

namespace tket {

/**
 * General information about an \p OpType
 */
struct OpTypeInfo {
  const std::string name;       /**< name */
  const std::string latex_name; /**< name in Latex representation */

  /** Number of phase parameters */
  unsigned n_params() const { return param_mod.size(); }

  /**
   * Moduli of parameters.
   *
   * This is a vector whose i'th entry is the least n > 0 such that adding n
   * to the i'th parameter leaves the operation unchanged.
   */
  const std::vector<unsigned> param_mod;

  const std::optional<op_signature_t>
      signature; /** types of inputs and outputs */
};

/** Information including name and shape of each operation type */
TKET_EXPORT const std::map<OpType, OpTypeInfo> &optypeinfo();

/** Operation type not valid in the current context */
class BadOpType : public std::logic_error {
 public:
  BadOpType(const std::string &message, OpType optype)
      : std::logic_error(message + ": " + optypeinfo().at(optype).name) {}
  explicit BadOpType(OpType optype) : BadOpType("Bad operation type", optype) {}
};

}  // namespace tket
