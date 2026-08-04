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

#include "tket/Mapping/RoutingMethod.hpp"
#include "tket_export.h"

namespace tket {

class TKET_EXPORT LexiRouteRoutingMethod : public RoutingMethod {
 public:
  /**
   * Checking and Routing methods redefined using LexiRoute. Only circuit depth,
   * corresponding to lookahead, is a required parameter.
   *
   * @param _max_depth Number of layers of gates checked inr outed subcircuit.
   */
  LexiRouteRoutingMethod(unsigned _max_depth = 100);

  /**
   * @param mapping_frontier Contains boundary of routed/unrouted circuit for
   * modifying
   * @param architecture Architecture providing physical constraints
   *
   * @return True if modification made, map between relabelled Qubit, always
   * empty.
   *
   */
  std::pair<bool, unit_map_t> routing_method(
      MappingFrontier_ptr& mapping_frontier,
      const ArchitecturePtr& architecture) const override;

  /**
   * @return Max depth used in lookahead
   */
  unsigned get_max_depth() const;

  nlohmann::json serialize() const override;

  static LexiRouteRoutingMethod deserialize(const nlohmann::json& j);

 private:
  unsigned max_depth_;
};

JSON_DECL(LexiRouteRoutingMethod);

}  // namespace tket
