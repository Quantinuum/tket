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

#include "tket/Architecture/Architecture.hpp"
#include "tket/Circuit/Circuit.hpp"
#include "tket/Mapping/RoutingMethod.hpp"
#include "tket/Utils/UnitID.hpp"

#include "tket_export.h"

namespace tket {

// list of error types to throw out
class MappingManagerError : public std::logic_error {
 public:
  explicit MappingManagerError(const std::string& message)
      : std::logic_error(message) {}
};

class TKET_EXPORT MappingManager {
 public:
  /* Mapping Manager Constructor */
  // MappingManager object defined by Architecture initialised with
  MappingManager(const ArchitecturePtr& _architecture);

  /**
   * route_circuit
   * Referenced Circuit modified such that all multi-qubit gates are permitted
   * by this->architecture_ RoutingIncompatibility thrown if Circuit has more
   * logical qubits than Architecture has physical qubits RoutingIncompatibility
   * thrown if Circuit has a gate of OpType not in Architecture's permitted
   * OpTypes
   *
   * @param circuit Circuit to be routed
   * @param routing_methods Ranked RoutingMethod objects to use for routing
   * segments.
   * @return True if circuit is modified
   */
  bool route_circuit(
      Circuit& circuit,
      const std::vector<RoutingMethodPtr>& routing_methods) const;

  /**
   * route_circuit_maps
   * Referenced Circuit modified such that all multi-qubit gates are permitted
   * by this->architecture_ RoutingIncompatibility thrown if Circuit has more
   * logical qubits than Architecture has physical qubits RoutingIncompatibility
   * thrown if Circuit has a gate of OpType not in Architecture's permitted
   * OpTypes
   *
   * @param circuit Circuit to be routed
   * @param routing_methods Ranked RoutingMethod objects to use for routing
   * segments.
   * @param maps For tracking placed and permuted qubits during Compilation
   * @return True if circuit is modified
   */
  bool route_circuit_with_maps(
      Circuit& circuit, const std::vector<RoutingMethodPtr>& routing_methods,
      std::shared_ptr<unit_bimaps_t> maps) const;

 private:
  ArchitecturePtr architecture_;
};
}  // namespace tket
