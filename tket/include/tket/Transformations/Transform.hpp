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

#include <functional>
#include <memory>

#include "tket/Circuit/Circuit.hpp"

#include "tket_export.h"

namespace tket {

/**
 * A transformation of a circuit that preserves its semantics
 */
class TKET_EXPORT Transform {
 public:
  /**
   * A function that takes a circuit and (optionally) a relabelling of units.
   *
   * The relabelling, if present, maps the original unit IDs at the beginning
   * and end of the circuit to new names, which may be in a different order at
   * the beginning and end.
   *
   * The function returns false if no changes are made, otherwise true.
   */
  typedef std::function<bool(Circuit&, std::shared_ptr<unit_bimaps_t>)>
      Transformation;

  /**
   * A function that takes a circuit and does not rename any units.
   *
   * The function returns false if no changes are made, otherwise true.
   */
  typedef std::function<bool(Circuit&)> SimpleTransformation;

  typedef std::function<unsigned(const Circuit&)> Metric;

  /** The transformation applied. */
  Transformation apply_fn;

  /** Construct from a transformation function */
  explicit Transform(const Transformation& trans) : apply_fn(trans) {}

  /** Construct from a transformation function that preserves unit IDs */
  explicit Transform(const SimpleTransformation& trans)
      : apply_fn([=](Circuit& circ, std::shared_ptr<unit_bimaps_t>) {
          return trans(circ);
        }) {}

  /**
   * Apply the transform to a circuit
   *
   * @param circ circuit to be transformed
   *
   * @return whether any changes were made
   */
  bool apply(Circuit& circ) const { return apply_fn(circ, nullptr); }

  /**
   * Compose two transforms in sequence
   *
   * @param[in] lhs first transform
   * @param[in] rhs second transform
   *
   * @return the composite transform
   */
  friend TKET_EXPORT Transform operator>>(const Transform& lhs, const Transform& rhs);
};

TKET_EXPORT Transform operator>>(const Transform& lhs, const Transform& rhs);

namespace Transforms {

/**
 * Identity transform (does nothing, returns false)
 */
inline const Transform id =
    Transform([](Circuit&, std::shared_ptr<unit_bimaps_t>) { return false; });

/**
 * @brief A simple struct to store some two-qubit gate fidelities.
 *
 * We currently support CX, ZZMax and ZZPhase.
 *
 * This is meant to be easily extensible when further gate types will be
 * supported.
 *
 */
struct TwoQbFidelities {
  std::optional<double> CX_fidelity;
  std::optional<double> ZZMax_fidelity;
  std::optional<std::variant<double, std::function<double(double)>>>
      ZZPhase_fidelity;
};

}  // namespace Transforms

}  // namespace tket
