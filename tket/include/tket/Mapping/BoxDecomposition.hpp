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

#include "tket/Mapping/MappingFrontier.hpp"
#include "tket/Mapping/RoutingMethod.hpp"

#include "tket_export.h"

namespace tket {

class BoxDecomposition {
 public:
  /**
   * Class Constructor
   * @param _architecture Architecture object added operations must respect
   * @param _mapping_frontier Contains Circuit object to be modified
   */
  BoxDecomposition(
      const ArchitecturePtr& _architecture,
      MappingFrontier_ptr& _mapping_frontier);

  /**
   * Decompose any boxes in the next slice after the frontier
   *
   * @return True if Box is decomposed
   */
  bool solve();

 private:
  // Architecture all new physical operations must respect
  ArchitecturePtr architecture_;
  MappingFrontier_ptr mapping_frontier_;
};

class TKET_EXPORT BoxDecompositionRoutingMethod : public RoutingMethod {
 public:
  /**
   * Decompose any boxes on the frontier
   */
  BoxDecompositionRoutingMethod();

  /**
   * @param mapping_frontier Contains boundary of routed/unrouted circuit for
   * modifying
   * @param architecture Architecture providing physical constraints
   * @return Logical to Physical mapping at boundary due to modification.
   *
   */
  std::pair<bool, unit_map_t> routing_method(
      MappingFrontier_ptr& mapping_frontier,
      const ArchitecturePtr& architecture) const override;

  nlohmann::json serialize() const override;

  static BoxDecompositionRoutingMethod deserialize(const nlohmann::json& /*j*/);
};

}  // namespace tket
