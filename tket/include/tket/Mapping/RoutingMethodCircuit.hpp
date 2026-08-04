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

class TKET_EXPORT RoutingMethodCircuit : public RoutingMethod {
 public:
  virtual ~RoutingMethodCircuit() {}
  /**
   * RoutingMethodCircuit objects hold methods for partially routing subcircuits
   * in the incremental routing of full circuits.
   *
   * @param _route_subcircuit Function ptr for partial routing method
   * @param _max_size Max number of gates in partial routing circuit
   * @param _max_depth Max depth of partial routing circuit
   */
  RoutingMethodCircuit(
      const std::function<std::tuple<bool, Circuit, unit_map_t, unit_map_t>(
          const Circuit&, const ArchitecturePtr&)>
          _route_subcircuit,
      unsigned _max_size, unsigned _max_depth);

  /**
   * @param mapping_frontier Contains boundary of routed/unrouted circuit for
   * modifying
   * @param architecture Architecture providing physical constraints
   * @return Logical to Physical mapping at boundary due to modification.
   *
   */
  std::pair<bool, unit_map_t> routing_method(
      MappingFrontier_ptr& mapping_frontier,
      const ArchitecturePtr& architecture) const;

 private:
  const std::function<std::tuple<bool, Circuit, unit_map_t, unit_map_t>(
      const Circuit&, const ArchitecturePtr&)>
      route_subcircuit_;
  unsigned max_size_, max_depth_;
};

JSON_DECL(RoutingMethod);

}  // namespace tket
