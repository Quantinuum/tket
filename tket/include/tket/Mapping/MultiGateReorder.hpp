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

class MultiGateReorder {
 public:
  /**
   * Class Constructor
   * @param _architecture Architecture object added operations must respect
   * @param _mapping_frontier Contains Circuit object to be modified
   */
  MultiGateReorder(
      const ArchitecturePtr& _architecture,
      MappingFrontier_ptr& _mapping_frontier);

  /**
   * Try to commute any multi-qubit gates to the quantum frontier
   * @param max_depth Maximum number of layers of gates checked for simultaneous
   * commutation.
   * @param max_size Maximum number of gates checked for simultaneous
   * commutation.
   *
   * @return true if modification made
   */
  bool solve(unsigned max_depth, unsigned max_size);

 private:
  // Architecture all new physical operations must respect
  ArchitecturePtr architecture_;
  MappingFrontier_ptr mapping_frontier_;
  EdgeVec u_frontier_edges_;
};

class TKET_EXPORT MultiGateReorderRoutingMethod : public RoutingMethod {
 public:
  /**
   * Checking and Routing methods redefined using MultiGateReorder.
   * @param _max_depth Maximum number of layers of gates checked for
   * simultaneous commutation.
   * @param _max_size Maximum number of gates checked for simultaneous
   * commutation.
   */
  MultiGateReorderRoutingMethod(
      unsigned _max_depth = 10, unsigned _max_size = 10);

  /**
   * @param mapping_frontier Contains boundary of routed/unrouted circuit for
   * modifying
   * @param architecture Architecture providing physical constraints
   * @return Whether circuit is modified and Logical to Physical mapping at
   * boundary due to modification (always empty)
   *
   */
  std::pair<bool, unit_map_t> routing_method(
      MappingFrontier_ptr& mapping_frontier,
      const ArchitecturePtr& architecture) const override;

  nlohmann::json serialize() const override;

  static MultiGateReorderRoutingMethod deserialize(const nlohmann::json& j);

  /**
   * @return Maximum number of layers of gates checked for simultaneous
   * commutation.
   */
  unsigned get_max_depth() const;

  /**
   * @return Maximum number of gates checked for simultaneous commutation.
   */
  unsigned get_max_size() const;

 private:
  unsigned max_depth_;
  unsigned max_size_;
};

}  // namespace tket
