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

#include "tket/ArchAwareSynth/SteinerTree.hpp"
#include "tket/Mapping/MappingFrontier.hpp"
#include "tket/Mapping/RoutingMethod.hpp"
#include "tket_export.h"

namespace tket {

class AASRouteError : public std::logic_error {
 public:
  explicit AASRouteError(const std::string& message)
      : std::logic_error(message) {}
};

// Child class of RoutingMethod, with overloaded methods for routing
// MappingFrontier objects
class TKET_EXPORT AASRouteRoutingMethod : public RoutingMethod {
 public:
  /**
   * Checking and Routing methods for phase poly boxes using architecture aware
   * synthesis
   * @param _aaslookahead lookahead that should be used in the aas routing
   * @param _cnotsynthtype type of cnot synthesis that should be used
   */
  AASRouteRoutingMethod(
      unsigned _aaslookahead,
      aas::CNotSynthType _cnotsynthtype = aas::CNotSynthType::Rec);

  /**
   * @param mapping_frontier Contains boundary of routed/unrouted circuit for
   * modifying
   * @param architecture Architecture providing physical constraints
   * @return bool if the method has been executed and logical to Physical
   * mapping at boundary due to modification.
   *
   */
  std::pair<bool, unit_map_t> routing_method(
      std::shared_ptr<MappingFrontier>& mapping_frontier,
      const ArchitecturePtr& architecture) const override;

  /**
   * @return cnot synth type of this routing method
   */
  aas::CNotSynthType get_cnotsynthtype() const;

  /**
   * @return aaslookahead of this routing method
   */
  unsigned get_aaslookahead() const;

  nlohmann::json serialize() const override;

  static AASRouteRoutingMethod deserialize(const nlohmann::json& j);

 private:
  // type of cnot synthesis that should be used
  aas::CNotSynthType cnotsynthtype_;
  // lookahead that should be used in the aas routing
  unsigned aaslookahead_;
};
}  // namespace tket
